# gpsClock
Analog clock built upon Arduino that displays time based on GNSS (GPS and other satelite positioning systems) time messages. It has the following advantages:
* No need to set the clock. Once the GNSS receiver starts parsing GNSS time messages the clock automatically displays current time and date. Local timezone can be set with a parameter in the Arduino code.
* No need to make the clock exposed to the GNSS signals all the time. The adopted GNSS receiver continues generating GNSS time messages even after it cannot receive GNSS signal any longer.

## Characteristics
* Current implementation adopts SparkFun Pro Micro (https://www.sparkfun.com/products/12640). However, one can build the clock with any other Arduino board with enough number of ports.
* It uses GY-NEO6MV2 GNSS (GPS) board on which ublox NEO-6M module is mounted. All NEO-6M modules have internal battery so can it can hold satelite tracking information for short amount of time without external power supply. In addition, the board is equipped with a ceramic antenna and thus can work standalone with adequate power supply.
* To avoid complicated gear trains, the clock uses separate motors for all of the hands. That is, only one hand is moved by a motor. Note that, the adoption of multiple motors do not have major impact on the battery lifetime of the clock because Arduino (even in idle state) is.
* Hall sensors and magnets are used to align the hands to the base positions.
* While handling date values in deriving local time from UTC one provided by the GNSS module, last day of each month rules are precisely considered. Especially, for the last day of February, it completely follows Gregorian calender.
* Though no hand is assigned to month and year, those time values are tracked internally and can be displayed with minimum effort of adding a motor and hand or a display.

## Software mechanism
The SW part is composed of mainly two parts. One part collects NMEA (National Marine Electronics Association) messages from the GNSS module and parses current UTC time and date from it. The parsed UTC time and date values then are converted to the local ones and stored in global variables. The other part derives the differences from the local time and date to the current position of hands and moves the hands accordingly. Current positions of the hands are also stored in a set of global variables. Below two subsections elaborate more on each part.

### NMEA messages parser
The adopted GNSS module GY-GPS6MV2 (seems to be a dead copy of GY-NEO6MV6) communicates with SparkFun Pro Micro via the secondary (N.B. USB port is the primary one) serial communication bus isserial communication bus which is mapped to `TXO` and `RXI` pins. Once 5V power is provided to the module, it periodically transmits NMEA messages in text like below even when the module is not locked to any GNSS satelite.

```
$BDGSA,A,1,,,,,,,,,,,,,,,,4*17
$GPGSV,0,1,00*78
$BDGSV,0,1,00*69
$GNRMC,,V,,,,,,,,,,N*4D
$GNZDA,,,,,,*56
$GNGGA,,,,,,0,00,,,M,,M,,*78
$GPGSA,A,1,,,,,,,,,,,,,,,,1*03
$BDGSA,A,1,,,,,,,,,,,,,,,,4*17
$GPGSV,0,1,/0*78
$BDGSV,0,1,00*69
```

The messages are in NMEA format. Each line represents a sperate NMEA message, and is terminated by one carriage return (<CR>) and one line feed (<LR>) characters in series. Every NMEA message is in the form of `$[GnssType - 2 char's][MessageType: 3-char's],[field_1],[field_2],...,[field_n]*[Checksum: 2-chars]`. Below is a brief description of fields of each message.
* `GnssType` represents the type of GNSS the message is extracted from. `GP` for GPS, `GN` for GLONASS, `BD` for Beidou, etc.
* `MessageType` represents the kind of information the message contains. For instance, `GGA` messages contain overview of information from the corresponding GNSS including UTC time and position of the receiver, `GSV` the list of GNSS satelites to which the receiver is locked to, and `ZDA` UTC time and date.
* `field_i` are the actual data of each message. `MessageType` and each field is delimited by a comma.
* `Checksum`, prefixed by an asterisk is a two-digit hex number (i.e. 1 byte), which is derived by cumulative XOR of all preceding characters.

Note that in the above example NMEA messages lots of fields are empty. Below are example NMEA messages when the module is locked to at least one GNSS. Notice that multiple fields are no longer void.

```
$GPGSV,2,2,06,23,,,,24,,,*78
$BDGSV,2,1,05,2,,,,4,,,,1,,,,3,,,*6A
$BDGSV,2,2,05,5,,,*58
$GNRMC,032307.514,V,,,,,,,191023,,,N*5E
$GNZDA,032307.514,19,10,2023,,*47
$GNGGA,032308.514,,,,,0,00,+<M,,M,,*6C
```

Among various types of NMEA messages the parser only targets ZDA messages and once its checksum is verified it extracts UTC minute, hour, and date. For instance, the ZDA message in the above example, `$GNZDA,032307.514,19,10,2023,,*47`, contains current UTC hour (03), miute (23), second (07), date (19), month (10), and year (2023). These UTC time values are then further converted into those of the local timezone selected by a global variable `localOffset2Utc`. One thing worth to be noted in UTC-to-local conversion is the date. Unlike minute and hour, it is quite tricky to determine what the next/previous date is when the UTC date is the start/end date of the month. It gets even complicated when it comes to a leap year. This complicated task is implemented in `refineDateMonthYear` with lots of branches.

### Hand-moving mechanism
gpsClock does not have any menas for tracking instantaneous position of each hand. Instead, Hall sensors are attached to appropriated positions of the clock back plate and magnets to the back of each hand. On boot, each hand rotates until one is at its base position then each hand starts moving accordingly to the time values. The sensors are also used for aligning each hand when each hand hits its base time value (0 minute, 0/12 hour, and 1st date). This mechanism enables the hands to anchor to the base positions so that position erros do not build up with lack of any measures for hand-position measurement.

The differences in time or date values between the local time/date and current hand positions are converted to the number of steps the stepper motors connected to the hands should rotate. The number of steps per unit time or date differs not only by the hand but also by the hand positions to compensate gravitational forces applied to the hands. Below is an example code snippet that determines the number of steps to be rotated for the minute hand.

```
#define MIN_UNIT_STEPS 36 // ad hoc values due to cheap inaccurate stepper
...

if (localTimeDateMonthYearHand[MINUTE] % 6 == 1 && localTimeDateMonthYearHand[MINUTE] <= 30)
    minStepper.step(MIN_UNIT_STEPS + 1); // fine adjustment
else if (localTimeDateMonthYearHand[MINUTE] % 2 == 1 && localTimeDateMonthYearHand[MINUTE] > 30)
    minStepper.step(MIN_UNIT_STEPS + 1); // fine adjustment
else
    minStepper.step(MIN_UNIT_STEPS);
```

Here `localTimeDateMonthYearHand[MINUTE]` represents current minute hand position, and `minStepper.step(...)` moves the stepper motor for the minute hand. The key idea here is to derive integer number of steps that can best represent natual movement of the minute hand because the stepper motor cannot move less than one step (N.B. I did not consider microstepping because the adopted stepper motors---28BYJ-48---were quite inaccurate even when it moved by integer steps). The branch is mainly composed of two parts: one for moving the hand within the first semicircle (i.e. 0--30 min.) and the other for the second one so that the number of steps can be taken differently depending on whether the hand is descending or ascending. The conditions with the remainder operator makes up a practical non-integer number of steps for moving the hand, which would be a fine adjustment.
To take up the cases that the hands should move more than one unit (i.e. the differences between the local time and current hand position is more than one). The above conditional statement is inside an external loop which reduces the difference by one at a time until it gets to zero.

## Power Consumption and Battery Time
Although the most dominant instantaneous power drain is from motors, GNSS module also takes up significant portion of average power drainage. Below are comparisons of power consumption depending on the SW running on Arduino and GNSS module connection.

| Config | Power Consumption |
| ------ | ----------------- |
| Blank code running on Pro Micro without any connection | 0.14W |
| Blank code with GNSS module | 0.39W |
| gpsClock code without any connection | 0.24W |
| gpsClock code with GNSS module | 0.44W |

As can be seen GNSS module takes up pretty much power. For instance, a blank Pro Micro and GNSS module alone consume 9.36Wh a day. This increases a little bit to 10.56Wh when Pro Micro is running gpsClock SW. Few dry cells can hold days under this much of power consumption. Below are battery times of gpsClock with various types of batterys without considering any overhead from voltage conversion, mechanical loss, motor driving, etc.

| Battery | Output Voltage (V) | Capacity (mAh) | Capacity (Wh) | Upper Bound of Battery Time |
| ------- | ------------------ | -------------- | ------------- | --------------------------- |
| 9V (Alkaline) | 550 | 4.95 | 11.3 |
| C (Alkaline) | 1.5 | 8,000 | 12 | 27.3 |
| D (Alkaline) | 1.5 | 12,000--18,000 | 18--27 | 40.9--61.4 |
| 18650 (Li-Ion) | 3.7 | 3,500 | 12.95 | 29.4 |
| 2S1P 21700 module | 7.26 | 4,850 | 35.2 | 80 |

Considering the significant amount of power consumption, 2S1P 21700 module was chosen. Including power drainage for motors it actually lasts about 50+ hours.

## Parts arrangement and Schematic
### Circuits Schematic
![gpsClock Schematic](/images/gpsClockSchematic.svg)

### Cross Section of Mechanical Structure
![gpsClock Cross Section](/image/gpsClockCrossSection.png)

### 3D model of clock frame and hands 
All models are drawn by [Tinkercad](https://www.tinkercad.com/). For CNC works or 3D printing, you can download STL files in Tinkercad pages.
* Assembed view: https://www.tinkercad.com/things/5qHVLQV69g6?sharecode=M-N8xg26T4Tp7vN6dTPjyoYMho2iMjWcLH_KvJiNK8Y
* Front and back plate: https://www.tinkercad.com/things/elm0u6JZKUI?sharecode=4npgwWMFvZ8sjh_XxynUjGjd_OhpR7mrLe-HZ6zU8vI
* Hands: https://www.tinkercad.com/things/gcw43MZUidC?sharecode=QKOi6mAoi5HatGKXw9UPfKnBdrYteMXhOlwsAzMnTao

### Assembled Clock and Parts Arrangement
![gpsClock Parts Arrangement](/images/gpsClockPhoto.png)

### Bill Of Materials
| Part Name | Quantity | Description |
| --------- | -------- | ----------- |
| SparkFun Pro Micro | 1 | Microcontroller<br>Any Arduino with enough ports works |
| GY-GPS6MV2<br>(or GY-NEO6MV6) | 1 | GNSS module (antenna included)<br>Any module with NMEA output over serial works |
| DC-DC Step Down Converter (5V) | 1 | 5V DC supply<br>Any converter with enough capacity works |
| Stepper Motor | 3 | 28BYJ-48<br>Any stepper with enough resolution works |
| Motor Support | 6 | 25mm long 3φ bolts and 3φ nuts |
| Stepper Driver | 3 | Stepper Driver for 28BYJ-48 Stepper (ULN2003-based)<br>Any driver compatible with the stepper works |
| 48DP 48T Spur Gear | 2 | Any gear train for driving coaxial hands works (frame re-design necessary)<br>[Reference product](https://www.aliexpress.com/item/3256804662258829.html?spm=a2g0o.detail.0.0.3f24k0fLk0fL7R&gps-id=pcDetailTopMoreOtherSeller&scm=1007.40050.281175.0&scm_id=1007.40050.281175.0&scm-url=1007.40050.281175.0&pvid=a824a2c5-baf2-4131-8d67-155d0a6c4c03&_t=gps-id%3ApcDetailTopMoreOtherSeller%2Cscm-url%3A1007.40050.281175.0%2Cpvid%3Aa824a2c5-baf2-4131-8d67-155d0a6c4c03%2Ctpp_buckets%3A668%232846%238114%231999&pdp_npi=3%40dis%21KRW%212945.0%212007.0%21%21%21%21%21%40214100e416897460541663389e990b%2112000030731131390%21rec%21KR%21824356678&gatewayAdapt=4itemAdapt) |
| 48DP 45T Spur Gear | 2 | Any gear train for driving coaxial hands works (frame re-design necessary)<br>[Reference product](https://www.aliexpress.com/item/3256804662258829.html?spm=a2g0o.detail.0.0.3f24k0fLk0fL7R&gps-id=pcDetailTopMoreOtherSeller&scm=1007.40050.281175.0&scm_id=1007.40050.281175.0&scm-url=1007.40050.281175.0&pvid=a824a2c5-baf2-4131-8d67-155d0a6c4c03&_t=gps-id%3ApcDetailTopMoreOtherSeller%2Cscm-url%3A1007.40050.281175.0%2Cpvid%3Aa824a2c5-baf2-4131-8d67-155d0a6c4c03%2Ctpp_buckets%3A668%232846%238114%231999&pdp_npi=3%40dis%21KRW%212945.0%212007.0%21%21%21%21%21%40214100e416897460541663389e990b%2112000030731131390%21rec%21KR%21824356678&gatewayAdapt=4itemAdapt) |
| 3φ Hallow Shaft | 1 | Plastic or metal<br>Outer diameter 3mm and Inner 2+mm<br>Inner diameter should be enough to fit 2φ solid shaft inside<br>[Reference product](https://www.aliexpress.com/item/3256805114412770.html?spm=a2g0o.store_pc_groupList.8148356.1.57341d2chcQfhT&pdp_npi=3%40dis%21KRW%21%E2%82%A9+2%2C966%21%E2%82%A9+445%21%21%21%21%21%402101c59816897621335375041eb902%2112000033103487030%21sh%21KR%21824356678&gatewayAdapt=4itemAdapt) |
| 2φ Solid Shaft | 3 | Plastic or metal<br>Outer diameter 2mm<br>[Reference product](https://www.aliexpress.com/item/3256805114412770.html?spm=a2g0o.store_pc_groupList.8148356.1.57341d2chcQfhT&pdp_npi=3%40dis%21KRW%21%E2%82%A9+2%2C966%21%E2%82%A9+445%21%21%21%21%21%402101c59816897621335375041eb902%2112000033103487030%21sh%21KR%21824356678&gatewayAdapt=4itemAdapt) |
| Hall Sensor | 3 | A3144-based module<br>Any Hall sensor with enough sensitivity and capped output of 5V works  |
| Neodymium Magnet | 6 | Attached on the back side of each hand<br>Multiple magnets can be attached on one hand to compensate the gap between the hand and the frame<br>Any magnet with enough strength works |
| 7.4V Li-Ion Battery | 1 | Any battery higher than 5V with enough capcacity works<br>Using connectors for easy battery replacement is recommended |
| Clock Frame | 1 | Front and back plate<br>STL file can be downloaded from [Tinkercad](#3D-model-of-clock-frame-and-hands)<br>Anything that can hold the other parts in place works |
| Frame Support | 4 | Connected one 50mm 3φ screw female-female support and one 45mm 3φ screw male-female support to support one corner of the frame<br>10mm long 3φ bolts to fix the supports and plates in place |
| Clock Hands Set | 1 | Hour, minute, and date hands<br>STL file can be downloaded from [Tinkercad](#3D-model-of-clock-frame-and-hands)<br>Any hands that can be fixed on the shafts work |

## Video
