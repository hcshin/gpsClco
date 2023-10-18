# gpsClock
Analog clock built upon Arduino that displays time based on GNSS (GPS and other satelite positioning systems) time messages. It has the following advantages:
* No need to set the clock. Once the GNSS receiver starts parsing GNSS time messages the clock automatically displays current time and date. Local timezone can be set with a parameter in the Arduino code.
* No need to make the clock exposed to the GNSS signals all the time. The adopted GNSS receiver continues generating GNSS time messages even after it cannot receive GNSS signal any longer.

## Characteristics
* Current implementation adopts SparkFun Pro Micro (https://www.sparkfun.com/products/12640). However, one can build the clock with any other Arduino board with enough number of ports.
* It uses GY-NEO6MV2 GNSS (GPS) board on which ublox NEO-6M module is mounted. All NEO-6M modules have internal battery so can it can hold satelite tracking information for short amount of time without external power supply. In addition, the board is equipped with a ceramic antenna and thus can work standalone with adequate power supply.
* To avoid complicated gear trains, the clock uses separate motors for all of the hands. That is, only one hand is moved by a motor. Note that, the adoption of multiple motors do not have major impact on the battery lifetime of the clock because Arduino (even in idle state) is.
* Hall sensors and magnets are used to align the hands to the base positions. The sensors are attached to appropriated positions of the clock back plate and magnets to the back of each hand. On boot, each hand rotates until one is at its base position then each hand starts moving accordingly to the time values. The sensors are also used for aligning each hand when each hand hits its base time value (0 minute, 0/12 hour, and 1st date). This mechanism enables the hands to anchor to the base positions so that position erros do not build up with lack of any measures for hand-position measurement.
* While handling date values in deriving local time from UTC one provided by the GNSS module, last day of each month rules are precisely considered. Especially, for the last day of February, it completely follows Gregorian calender.
* Though no hand is assigned to month and year, those time values are tracked internally and can be displayed with minimum effort of adding a motor and hand or a display.

## Software mechanism
The SW part is composed of mainly two parts. One part collects NMEA (National Marine Electronics Association) messages from the GNSS module and parses current UTC time and date from it. The parsed UTC time and date values then are converted to the local ones and stored in global variables. The other part derives the differences from the local time and date to the current position of hands and moves the hands accordingly. Current positions of the hands are also stored in a set of global variables. Below two subsections elaborate more on each part.

### NMEA messages parser

### Hand-moving mechanism
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
To take up the cases that the hands should move more than one unit (i.e. the differences between the local time and current hand position is not one. The above conditional statement is inside an external loop which reduces the difference by one at a time until it gets to zero.

## Schematic 3D modeling
![gpsClock Schematic](/images/gpsClockSchematic.svg)

## Clock frame and other hardware
### 3D model of clock frame and hands 
All models are drawn by [Tinkercad](https://www.tinkercad.com/). For CNC works or 3D printing, you can download STL files in Tinkercad pages.
* Assembed view: https://www.tinkercad.com/things/5qHVLQV69g6?sharecode=M-N8xg26T4Tp7vN6dTPjyoYMho2iMjWcLH_KvJiNK8Y
* Front and back plate: https://www.tinkercad.com/things/elm0u6JZKUI?sharecode=4npgwWMFvZ8sjh_XxynUjGjd_OhpR7mrLe-HZ6zU8vI
* Hands: https://www.tinkercad.com/things/gcw43MZUidC?sharecode=QKOi6mAoi5HatGKXw9UPfKnBdrYteMXhOlwsAzMnTao

### List of hardware with description

## Video
