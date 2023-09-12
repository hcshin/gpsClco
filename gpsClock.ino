// Includes
#include <Stepper.h>

// Constants
// GPS
#define NMEA_MSGS_BUF_SIZE 250
#define ZDA_MSG_MAXLEN 37 // this does not count for '\0' in the end
#define INTER_NMEA_MSG_DELIM "\r\n"
#define INTRA_NMEA_MSG_DELIM ","
#define ZDA_UTC_HHMMSSSSS_FIELD_IDX 1
#define ZDA_UTC_DATE_FIELD_IDX 2
#define ZDA_UTC_MONTH_FIELD_IDX 3
#define ZDA_UTC_YEAR_FIELD_IDX 4
#define NUM_DAYS_BIG_MONTH
// Stepper
#define STEPPER_CONF_DELAY_MS 10
#define STEPS_PER_REV 2038
#define MOTOR_SPEED 10
#define HR_UNIT_STEPS 182 // ad hoc values due to cheap inaccurate stepper
#define MIN_UNIT_STEPS 39 // ad hoc values due to cheap inaccurate stepper
#define DATE_UNIT_STEPS 35 // ad hoc values due to cheap inaccurate stepper
#define MIN_PIN1 2
#define MIN_PIN2 3
#define MIN_PIN3 4
#define MIN_PIN4 5
#define HR_PIN1 6
#define HR_PIN2 7
#define HR_PIN3 8
#define HR_PIN4 9
#define DATE_PIN1 10
#define DATE_PIN2 16
#define DATE_PIN3 14
#define DATE_PIN4 15
// Hall Sensor
#define HR_ANCHOR_SENSOR_PIN A0
#define MIN_ANCHOR_SENSOR_PIN A1
#define DATE_ANCHOR_SENSOR_PIN A2
#define HALL_SENSOR_OFF_VALUE 1023


// enums
enum ENUM_TIME_DATE_MONTH_YEAR {HOUR = 0, MINUTE, DATE, MONTH, YEAR};
enum MOTOR_DESIG {HR_MOT = 0, MINUTE_MOT, DATE_MOT};

// Global Params
unsigned int debugMask = 0x1E0;
int localOffset2Utc = 9; // might be parsed by DIP switches later 

// Global Variables and Buffers
// GPS
char nmeaMsgsBuf[NMEA_MSGS_BUF_SIZE];
char nmeaMsgsBufForTokenize[NMEA_MSGS_BUF_SIZE];
char debugPrintBuf[NMEA_MSGS_BUF_SIZE];
unsigned int nmeaMsgsBufPos = 0;
int localTimeDateMonthYear[5] = {0, 0, 1, 1, 2023}; // {hour, minute, date, month, year}
int localTimeDateMonthYearHand[5] = {0, 0, 1, 1, 2023}; // Represents current hand positions
// Stepper
Stepper hrStepper = Stepper(STEPS_PER_REV, HR_PIN1, HR_PIN2, HR_PIN3, HR_PIN4);
Stepper minStepper = Stepper(STEPS_PER_REV, MIN_PIN1, MIN_PIN2, MIN_PIN3, MIN_PIN4);
Stepper dateStepper = Stepper(STEPS_PER_REV, DATE_PIN1, DATE_PIN2, DATE_PIN3, DATE_PIN4);

// Functions
// Common
void serialPrintf(char* fmtStr, ...) {
  va_list argList;
  va_start(argList, fmtStr);
  vsprintf(debugPrintBuf, fmtStr, argList);
  Serial.print(debugPrintBuf);
  va_end(argList);
}

// Stepper
void configureSteppers() {
  hrStepper.setSpeed(MOTOR_SPEED);
  minStepper.setSpeed(MOTOR_SPEED);
  dateStepper.setSpeed(MOTOR_SPEED);

  // align motors
  while (digitalRead(HR_ANCHOR_SENSOR_PIN) == HIGH) { // HIGH == Hall sensor off
    hrStepper.step(1);
    delay(STEPPER_CONF_DELAY_MS);
  } 
  while (digitalRead(MIN_ANCHOR_SENSOR_PIN) == HIGH) {
    minStepper.step(1);
    delay(STEPPER_CONF_DELAY_MS);
  }
  while (digitalRead(DATE_ANCHOR_SENSOR_PIN) == HIGH) {
    dateStepper.step(-1); // date stepper moves in retrograde manner
    delay(STEPPER_CONF_DELAY_MS);
  }

  // Turn off all steppers to save power
  digitalWrite(HR_PIN1, LOW);
  digitalWrite(HR_PIN2, LOW);
  digitalWrite(HR_PIN3, LOW);
  digitalWrite(HR_PIN4, LOW);

  digitalWrite(MIN_PIN1, LOW);
  digitalWrite(MIN_PIN2, LOW);
  digitalWrite(MIN_PIN3, LOW);
  digitalWrite(MIN_PIN4, LOW);

  digitalWrite(DATE_PIN1, LOW);
  digitalWrite(DATE_PIN2, LOW);
  digitalWrite(DATE_PIN3, LOW);
  digitalWrite(DATE_PIN4, LOW);
}

void activateMotorsByUnits(unsigned int motorDesig, int diffInUnits) {
  switch (motorDesig) {
    case HR_MOT:
      if (localTimeDateMonthYear[HOUR] == 0) {
        while (digitalRead(HR_ANCHOR_SENSOR_PIN) == HIGH) { // Use sensor info for zero minute position
          hrStepper.step(1);
          delay(STEPPER_CONF_DELAY_MS);
        }
        diffInUnits = 0;
        localTimeDateMonthYearHand[HOUR] = localTimeDateMonthYear[HOUR];
      }
      else { // Non-zero positions
        while (diffInUnits > 0) { // move hand repetitively until the diff becomes 0
          if (localTimeDateMonthYearHand[HOUR] % 2 == 1)
            hrStepper.step(HR_UNIT_STEPS + 2); // fine adjustment
          else
            hrStepper.step(HR_UNIT_STEPS);

          diffInUnits--;
          localTimeDateMonthYearHand[HOUR] = localTimeDateMonthYear[HOUR] - diffInUnits;
          if (localTimeDateMonthYearHand[HOUR] < 0) localTimeDateMonthYearHand[HOUR] += 12;
        }
      }
     
      // turn off the stepper to save power
      digitalWrite(HR_PIN1, LOW);
      digitalWrite(HR_PIN2, LOW);
      digitalWrite(HR_PIN3, LOW);
      digitalWrite(HR_PIN4, LOW);
      break;

    case MINUTE_MOT:
      if (localTimeDateMonthYear[MINUTE] == 0) {
        while (digitalRead(MIN_ANCHOR_SENSOR_PIN) == HIGH) { // Use sensor info for zero minute position
          minStepper.step(1);
          delay(STEPPER_CONF_DELAY_MS);
        } 
        diffInUnits = 0;
        localTimeDateMonthYearHand[MINUTE] = localTimeDateMonthYear[MINUTE]; // Update hand position
      }
      else {
        while (diffInUnits > 0) {
          if (localTimeDateMonthYearHand[MINUTE] % 6 == 1 && localTimeDateMonthYear[MINUTE] <= 30)
            minStepper.step(MIN_UNIT_STEPS - 1); // fine adjustment
          else if (localTimeDateMonthYearHand[MINUTE] % 3 == 1 && localTimeDateMonthYear[MINUTE] > 30)
            minStepper.step(MIN_UNIT_STEPS + 1); // fine adjustment
          else
            minStepper.step(MIN_UNIT_STEPS);
          
          diffInUnits--;
          localTimeDateMonthYearHand[MINUTE] = localTimeDateMonthYear[MINUTE] - diffInUnits;
          if (localTimeDateMonthYearHand[MINUTE] < 0) localTimeDateMonthYearHand[MINUTE] += 60;
        }
      }

      

      // turn off the stepper to save power
      digitalWrite(MIN_PIN1, LOW);
      digitalWrite(MIN_PIN2, LOW);
      digitalWrite(MIN_PIN3, LOW);
      digitalWrite(MIN_PIN4, LOW);
      break;

    case DATE_MOT:
      if (diffInUnits < 0) { // Date only increses unless month changes.
        // Move back to day 1 position, then move until the hand reach current date (retrograde hand)
        // We don't need to compensate step angle for date hand because it does not circulate 
        while (digitalRead(DATE_ANCHOR_SENSOR_PIN) == HIGH) {
          dateStepper.step(-1); // reverse till day-1 position
          delay(STEPPER_CONF_DELAY_MS);
        }
      }
      else // Normal cases with no month change. The hand advances
        dateStepper.step(DATE_UNIT_STEPS * diffInUnits);

      diffInUnits = 0;
      localTimeDateMonthYearHand[DATE] = localTimeDateMonthYear[DATE]; // Update hand position

      digitalWrite(DATE_PIN1, LOW); // turn off the stepper to save power
      digitalWrite(DATE_PIN2, LOW);
      digitalWrite(DATE_PIN3, LOW);
      digitalWrite(DATE_PIN4, LOW);
      break;
  }
}

void moveHands() {
  // Derive the difference between current time and hand positions
  int minuteDiff = localTimeDateMonthYear[MINUTE] - localTimeDateMonthYearHand[MINUTE];
  int hourDiff = localTimeDateMonthYear[HOUR] - localTimeDateMonthYearHand[HOUR];
  int dateDiff = localTimeDateMonthYear[DATE] - localTimeDateMonthYearHand[DATE];
  // int monthDiff = localTimeDateMonthYear[MONTH] - localTimeDateMonthYearHand[MONTH]; // Currently no hand for month
  // no hand for year as well

  // Minute
  if (minuteDiff > 0)
    activateMotorsByUnits(MINUTE_MOT, minuteDiff);
  else if (minuteDiff < 0)
    activateMotorsByUnits(MINUTE_MOT, minuteDiff + 60); // Time only advances. Thus give +60 (one round of min hand) to current time if prev > current
  
  // Hour
  if (hourDiff > 0)
    activateMotorsByUnits(HR_MOT, hourDiff);
  else if (hourDiff < 0)
    activateMotorsByUnits(HR_MOT, hourDiff + 12); // Time only advances. Thus give +12 (one round of hr hand) to current time if prev > current
  
  // Date
  if (dateDiff != 0)
    activateMotorsByUnits(DATE_MOT, dateDiff); // Date is little tricky to handle. Offload it to another function.
}


void testMoveHands() { // set localTimeDateMonthYear in various manner to test moveHands
  // two rounds of minute hand
  for (int moveCount = 120; moveCount > 0; moveCount--) {
    localTimeDateMonthYear[MINUTE] = (localTimeDateMonthYearHand[MINUTE] + 1) % 60;
    if (debugMask & 0x100)
      serialPrintf("[Before moveHands] curr minute: %d, hand minute: %d\n", localTimeDateMonthYear[MINUTE], localTimeDateMonthYearHand[MINUTE]);
    
    moveHands();
    if (debugMask & 0x100)
      serialPrintf("[After moveHands] curr minute: %d, hand minute: %d\n", localTimeDateMonthYear[MINUTE], localTimeDateMonthYearHand[MINUTE]);
    
    delay(1000);
  }

  // two rounds of hour hand
  for (int moveCount = 24; moveCount > 0; moveCount--) {
    localTimeDateMonthYear[HOUR] = (localTimeDateMonthYearHand[HOUR] + 1) % 12;
    if (debugMask & 0x100)
      serialPrintf("[Before moveHands] curr hour: %d, hand hour: %d\n", localTimeDateMonthYear[HOUR], localTimeDateMonthYearHand[HOUR]);
    
    moveHands();
    if (debugMask & 0x100)
      serialPrintf("[After moveHands] curr hour: %d, hand hour: %d\n", localTimeDateMonthYear[HOUR], localTimeDateMonthYearHand[HOUR]);
    
    delay(1000);
  }

  // // two roundtrip of date hand
  for (int moveCount = 62; moveCount > 0; moveCount--) {
    localTimeDateMonthYear[DATE] = max((localTimeDateMonthYearHand[DATE] + 1) % 32, 1); // there's no date 0
    if (debugMask & 0x100)
      serialPrintf("[Before moveHands] curr date: %d, hand date: %d\n", localTimeDateMonthYear[DATE], localTimeDateMonthYearHand[DATE]);
    
    moveHands();
    if (debugMask & 0x100)
      serialPrintf("[After moveHands] curr date: %d, hand date: %d\n", localTimeDateMonthYear[DATE], localTimeDateMonthYearHand[DATE]);
    
    delay(1000);
  }

}

// GPS
bool isNmeaChecksumCorrect(const char* nmeaMsg, const unsigned int nmeaMsgLen) {
  char nmeaChecksumStrLiteral[3];

  unsigned int nmeaChecksum = 0;
  for (unsigned int i = 1; i < nmeaMsgLen - 3; i++) {
    nmeaChecksum ^= nmeaMsg[i];
  }
  sprintf(nmeaChecksumStrLiteral, "%02X", nmeaChecksum); // for strncmp with checksum in msg. print in uppercase to match checksum in msg

  if ((debugMask & 0x08) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("NMEA msg checksum debug info:\n"
                                  "- msg: %s\n"
                                  "- msg len: %d\n"
                                  "- 1st char: %c\n"
                                  "- msg type: %c%c%c\n"
                                  "- last 3 chars: %s\n"
                                  "- nmea checksum calculated: %s\n",
                                  nmeaMsg, nmeaMsgLen, nmeaMsg[0], nmeaMsg[3], nmeaMsg[4], nmeaMsg[5], nmeaMsg + nmeaMsgLen - 3, nmeaChecksumStrLiteral);

  return strncmp(nmeaMsg + nmeaMsgLen - 2, nmeaChecksumStrLiteral, 2) == 0; // Caution! 0 means string match
}

int getLastDateOfFeb(int year) {
  if (year % 400 == 0)
    return 29;
  if (year % 100 == 0)
    return 28;
  if (year % 4 == 0)
    return 29;
  
  return 28;
}

void refineDateMonthYear(int calib) {
  if (calib != 1 && calib != -1) {
    if ((debugMask & 0x80) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
    // if (debugMask & 0x80)
      serialPrintf("[refineDateMonthYear] allowed calib values are either +1 or -1 but %d given\n", calib);
    return; 
  }

  localTimeDateMonthYear[DATE] += calib;
  if (localTimeDateMonthYear[MONTH] == 1) { // JAN
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 31; // DEC 31st
      localTimeDateMonthYear[MONTH] = 12;
      localTimeDateMonthYear[YEAR]--;
    }
    else if (localTimeDateMonthYear[DATE] > 31) {
      localTimeDateMonthYear[DATE] = 1; // FEB 1st
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else if (localTimeDateMonthYear[MONTH] == 2) { // FEB
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 31; // JAN 31st
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > getLastDateOfFeb(localTimeDateMonthYear[YEAR])) {
      localTimeDateMonthYear[DATE] = 1; // MAR 1st
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else if (localTimeDateMonthYear[MONTH] == 3) { // MAR
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = getLastDateOfFeb(localTimeDateMonthYear[YEAR]); // FEB 28th or 29th
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > 31) {
      localTimeDateMonthYear[DATE] = 1; // APR 1st
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else if (localTimeDateMonthYear[MONTH] == 4 || 
           localTimeDateMonthYear[MONTH] == 6 || 
           localTimeDateMonthYear[MONTH] == 9 ||
           localTimeDateMonthYear[MONTH] == 11) { // when last date of this month is 30 and that of prev month is 31  
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 31;
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > 30) {
      localTimeDateMonthYear[DATE] = 1;
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else if (localTimeDateMonthYear[MONTH] == 5 || 
           localTimeDateMonthYear[MONTH] == 7 || 
           localTimeDateMonthYear[MONTH] == 10) { // when last date of this month is 31 and that of prev month is 30
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 30;
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > 31) {
      localTimeDateMonthYear[DATE] = 1;
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else if (localTimeDateMonthYear[MONTH] == 8) { // AUG: last date of both this and prev month are 31
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 31;
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > 31) {
      localTimeDateMonthYear[DATE] = 1;
      localTimeDateMonthYear[MONTH]++;
    }
  }
  else { // DEC
    if (localTimeDateMonthYear[DATE] < 1) {
      localTimeDateMonthYear[DATE] = 30;
      localTimeDateMonthYear[MONTH]--;
    }
    else if (localTimeDateMonthYear[DATE] > 31) {
      localTimeDateMonthYear[DATE] = 1; // JAN 1st
      localTimeDateMonthYear[MONTH] = 1;
      localTimeDateMonthYear[YEAR]++;
    }
  }
}

void refineLocalTimeDateMonthYear() {
  if (localTimeDateMonthYear[HOUR] > 23) {
    localTimeDateMonthYear[HOUR] = (localTimeDateMonthYear[HOUR] - 24) % 12;
    refineDateMonthYear(1);
  }
  else if(localTimeDateMonthYear[HOUR] < 0) {
    localTimeDateMonthYear[HOUR] = (localTimeDateMonthYear[HOUR] + 24) % 12;
    refineDateMonthYear(-1);
  }
  else
    localTimeDateMonthYear[HOUR] %= 12;
  
  if ((debugMask & 0x80) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
    serialPrintf("Refined local time values\n"
                  "- Hr: %d\n"
                  "- Min: %d\n"
                  "- Date: %d\n"
                  "- Month: %d\n"
                  "- Year: %d\n", 
                  localTimeDateMonthYear[HOUR], 
                  localTimeDateMonthYear[MINUTE], 
                  localTimeDateMonthYear[DATE],
                  localTimeDateMonthYear[MONTH],
                  localTimeDateMonthYear[YEAR]);
}

/*
void testRefineLocalTimeDateMonthYear() {
// boundry condition check
  serialPrintf("[JAN]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 1;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 1;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[FEB]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 28;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();
  
  serialPrintf("[MAR]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 3;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 3;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[APR]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 4;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 30;
  localTimeDateMonthYear[MONTH] = 4;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[MAY]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 5;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 5;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[JUN]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 6;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 30;
  localTimeDateMonthYear[MONTH] = 6;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[JUL]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 7;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 7;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[AUG]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 8;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 8;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[SEP]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 9;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 30;
  localTimeDateMonthYear[MONTH] = 9;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[OCT]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 10;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 10;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[NOV]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 11;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 30;
  localTimeDateMonthYear[MONTH] = 11;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  serialPrintf("[DEC]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 12;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 31;
  localTimeDateMonthYear[MONTH] = 12;
  localTimeDateMonthYear[YEAR] = 2023;
  refineLocalTimeDateMonthYear();
  
  // leap year check
  serialPrintf("\n<ordinary leap year>\n");
  serialPrintf("[FEB]\n");
  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 28;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2020;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 29;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2020;
  refineLocalTimeDateMonthYear();
  
  serialPrintf("[MAR]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 3;
  localTimeDateMonthYear[YEAR] = 2020;
  refineLocalTimeDateMonthYear();

  serialPrintf("\n<multiple-of-100 year>\n");
  serialPrintf("[FEB]\n");
  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 28;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2100;
  refineLocalTimeDateMonthYear();

  // No Feb 29th in 2100
  
  serialPrintf("[MAR]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 3;
  localTimeDateMonthYear[YEAR] = 2100;
  refineLocalTimeDateMonthYear();

  serialPrintf("\n<multiple-of-400 year>\n");
  serialPrintf("[FEB]\n");
  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 28;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2000;
  refineLocalTimeDateMonthYear();

  localTimeDateMonthYear[HOUR] = 35;
  localTimeDateMonthYear[DATE] = 29;
  localTimeDateMonthYear[MONTH] = 2;
  localTimeDateMonthYear[YEAR] = 2000;
  refineLocalTimeDateMonthYear();
  
  serialPrintf("[MAR]\n");
  localTimeDateMonthYear[HOUR] = -12;
  localTimeDateMonthYear[DATE] = 1;
  localTimeDateMonthYear[MONTH] = 3;
  localTimeDateMonthYear[YEAR] = 2000;
  refineLocalTimeDateMonthYear();
}
*/

void processNmeaMsg(const char* nmeaMsg) {
  unsigned int nmeaMsgLen = strlen(nmeaMsg);
  
  // filter out incorrect and non-ZDA msgs
  if (
      nmeaMsgLen < 6 ||
      nmeaMsg[0] != '$' ||
      nmeaMsg[nmeaMsgLen - 3] != '*' ||
      (strncmp(nmeaMsg + 3, "ZDA", 3) != 0)  // Caution! 0 means string match
  ) {
    if ((debugMask & 0x10) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) 
      serialPrintf("Incorrect or non-ZDA msg %s\n", nmeaMsg);
    return;
  }

  // filter out msgs with wrong checksum
  if (!isNmeaChecksumCorrect(nmeaMsg, nmeaMsgLen)) {
    if ((debugMask & 0x10) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) {
      serialPrintf("ZDA msg checksum not correct: %s\n", nmeaMsg);
    } 
    return; // do nothing: time and date value will not be updated
  }

  // Further tokenize ZDA msg to extract UTC time (hr & minute) and date
  char zdaMsg[ZDA_MSG_MAXLEN  + 1]; // +1 for null termination
  zdaMsg[ZDA_MSG_MAXLEN] = '\0'; // null termination
  memcpy(zdaMsg, nmeaMsg, min(nmeaMsgLen + 1, ZDA_MSG_MAXLEN)); // Copy ZDA msg buf because strtok corrupts the buffer contents
  if ((debugMask & 0x20) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("Copied ZDA msg: %s\n", zdaMsg);
  
  unsigned int zdaFieldIdx = 0;
  char* zdaField = strtok(zdaMsg, INTRA_NMEA_MSG_DELIM);
  if ((debugMask & 0x20) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("[ZDA msg token] %s\n", zdaField);
  // do nothing here because zdaFieldIdx == 0 is apparent (nothing useful in "$??ZDA")
  zdaFieldIdx++;
  
  while (zdaField != NULL) {
    zdaField = strtok(NULL, INTRA_NMEA_MSG_DELIM);
    if ((debugMask & 0x20) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("[ZDA msg token] %s\n", zdaField);

    if (zdaFieldIdx == ZDA_UTC_HHMMSSSSS_FIELD_IDX) { // parse UTC hour and min. hour value is still to be refined
      // check if zdaField has enough length (HHMMSS.SSSS thus 11)
      if (strlen(zdaField) != 10) {
        if ((debugMask & 0x40) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("ZDA time msg length is wrong: %s\n", zdaField);
        return;
      }
      
      char utcHh[3], utcMm[3];
      strncpy(utcHh, zdaField, 2); strncpy(utcMm, zdaField+2, 2);
      utcHh[2] = '\0'; utcMm[2] = '\0'; // null termination

      localTimeDateMonthYear[HOUR] = atoi(utcHh) + localOffset2Utc; // temporary value (may be negative or above 23)

      localTimeDateMonthYear[MINUTE] = atoi(utcMm); // this is ultimate (UTC min == local min always)
    }
    else if (zdaFieldIdx == ZDA_UTC_DATE_FIELD_IDX) { // parse UTC date. date value is still to be refined
      if (strlen(zdaField) != 2) {
        if ((debugMask & 0x40) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("ZDA date msg length is wrong: %s\n", zdaField);
        return;
      }

      char utcDate[3];
      strncpy(utcDate, zdaField, 2);
      utcDate[2] = '\0';

      localTimeDateMonthYear[DATE] = atoi(utcDate); // temporary value
    }
    else if (zdaFieldIdx == ZDA_UTC_MONTH_FIELD_IDX) { // parse UTC month. month value is still to be refined
      if (strlen(zdaField) != 2) {
        if ((debugMask & 0x40) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("ZDA month msg length is wrong: %s\n", zdaField);
        return;
      }

      char utcMonth[3];
      strncpy(utcMonth, zdaField, 2);
      utcMonth[2] = '\0';

      localTimeDateMonthYear[MONTH] = atoi(utcMonth); // temporary value
    }
    else if (zdaFieldIdx == ZDA_UTC_YEAR_FIELD_IDX) { // parse UTC year. year value is still to be refined
      if (strlen(zdaField) != 4) {
        if ((debugMask & 0x40) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2)) serialPrintf("ZDA year msg length is wrong: %s\n", zdaField);
        return;
      }

      char utcYear[5];
      strncpy(utcYear, zdaField, 4);
      utcYear[4] = '\0';

      localTimeDateMonthYear[YEAR] = atoi(utcYear); // temporary value
    }
    else if (zdaFieldIdx > ZDA_UTC_YEAR_FIELD_IDX)
      break; // not interested in values after UTC year

    zdaFieldIdx++;
  }
  
  if ((debugMask & 0x40) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
    serialPrintf("Parsed unrefined local time values\n"
                  "- Hr: %d\n"
                  "- Min: %d\n"
                  "- Date: %d\n"
                  "- Month: %d\n"
                  "- Year: %d\n", 
                  localTimeDateMonthYear[HOUR], 
                  localTimeDateMonthYear[MINUTE], 
                  localTimeDateMonthYear[DATE],
                  localTimeDateMonthYear[MONTH],
                  localTimeDateMonthYear[YEAR]);
    // refine local time values
    refineLocalTimeDateMonthYear();
}

void parseTimeAndDateFromNmeaMsgsBuf() {
  if ((debugMask & 0x02) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
    serialPrintf("----------NMEA Msgs Buf----------\n%s\n------------------------------\n", nmeaMsgsBuf);

  // Tokenize NMEA msgs
  memcpy(nmeaMsgsBufForTokenize, nmeaMsgsBuf, NMEA_MSGS_BUF_SIZE); // Copy NMEA msgs buf because strtok corrupts the buffer contents
  char* nmeaMsg = strtok(nmeaMsgsBufForTokenize, INTER_NMEA_MSG_DELIM);
  if ((debugMask & 0x04) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
    serialPrintf("[NMEA msg token] %s\n", nmeaMsg);

  // Process each NMEA msg
  processNmeaMsg(nmeaMsg);

  while (nmeaMsg != NULL) {
    nmeaMsg = strtok(NULL, INTER_NMEA_MSG_DELIM);
    if ((debugMask & 0x04) && (nmeaMsgsBufPos == NMEA_MSGS_BUF_SIZE - 2))
      serialPrintf("[NMEA msg token] %s\n", nmeaMsg);
    
    // Process each NMEA msg
    processNmeaMsg(nmeaMsg);
  }
}

void collectNmeaMsgs() {
  // receive incoming NMEA msgs
  if (Serial1.available()) {
    nmeaMsgsBuf[nmeaMsgsBufPos] = Serial1.read();
    nmeaMsgsBufPos = (nmeaMsgsBufPos + 1) % (NMEA_MSGS_BUF_SIZE - 1);
    nmeaMsgsBuf[nmeaMsgsBufPos] = '\0';
  }

  if (debugMask & 0x01) serialPrintf("NMEA msgs buf len: %d\n", strlen(nmeaMsgsBuf));
  
  // parse time and date from NMEA msgs
  parseTimeAndDateFromNmeaMsgsBuf();
}

void setup() {
  // Setup Serials
  Serial.begin(9600);
  Serial1.begin(9600);

  // configure steppers
  configureSteppers();

  // Wait it to be stable
  delay(2000);

  // Test codes
  // testMoveHands();
}

void loop() {
  // put your main code here, to run repeatedly:
  collectNmeaMsgs();
  moveHands();
}