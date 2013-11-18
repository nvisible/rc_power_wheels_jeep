// *****************************
// *    RC Power Wheels Jeep   *
// *****************************

/* 
 
 OVERVIEW
 A 12v dual-motor Power Wheels Jeep Hurricane will have drive 
 and steering remotely-controlled via Arduino.
 
 HARDWARE
 Power Wheels Jeep Hurricane
 Arduino UNO R3
 Circuits@Home USB Host Shield 2.0          http://www.circuitsathome.com/products-page/arduino-shields/usb-host-shield-2-0-for-arduino
 Pololu Simple Motor Controller 18v25       http://www.pololu.com/catalog/product/1381/resources
 Pololu JRK Motor Controller 12v12          http://www.pololu.com/catalog/product/1393/resources
 Pololu Concentric Linear Actuator          http://www.pololu.com/catalog/product/2319
 Xbox 360 Wireless USB Adapter
 Xbox 360 Wireless Controller
 
 HARDWARE CONFIGURATION
 
 * +-------+         +---------------+  USB    +-----------------+
 * |Arduino+-------->|USB Host Shield+-------->|XBOX USB Receiver|****//****XBOX Wireless Controller
 * +-+-----+         +---------------+         +-----------------+                  
 * |
 * | Tx (TTL Serial)
 * |
 * |------------------| 
 * |                  |
 * v Rx               v Rx
 * +----------+      +----------+
 * |Pololu SMC+      |Pololu Jrk|
 * +----------+      +----------+
 * Master            Slave
 * +                 +
 * |                 |
 * |                 |
 * v                 v
 * +--------+        +--------+
 * |Dual 12v|        |Linear  |
 * | Motors |        |Actuator|
 * +--------+        +--------+
 * 
 * NOTES
 * Outdoors with line-of-sight, the Wireless Xbox Controller has an awesome range. The max range
 * has not been fully tested, but I've gone over 50ft without any noticeable loss in signal.
 * 
 * Everything sits on board the Jeep. All TTL Serial connections are hard-wired into the Arduino 
 * and motor controllers. The Xbox Receiver is attached to a metal pole antenna-style.
 * 
 * Currently using a faster linear actuator that does 34lbs @ 1.7"/s 
 * http://www.pololu.com/catalog/product/2319
 * Would still like to use the 110lb actuator that I have now that the front steering has been redesigned.
 * There is a shorter throw in the new steering linkages and at times I've found the 34lb push 
 */

// *****Includes for the Circuits@Home USB Shield *****
#include <XBOXRECV.h>  // Xbox 360 Wireless Receiver

//Create USB instance? USB SHIELD
USB Usb;

// Create an instance of the Xbox Receiver inputs called XboxRCV
XBOXRECV XboxRCV(&Usb);  // USB SHIELD

// These next constants are bytes part of the Pololu Protocol
const byte pololuCommandByte = 170;
const byte smcDeviceNumber = 13;
const byte smcSpeedDataByte3 = 0;
const byte smcFWDbyte = 5;
const byte smcREVbyte = 6;
const byte jrkDeviceNumber = 11;

char smcSpeed;  // Final speed 
long int leftStickX; // Xbox Left Analog Stick value


void setup(){
  Serial.begin(9600);    // Serial baud rate to 9600

    // Halt program until shield is connected
  // USB.Init will return -1 for shield disconnected and 0 if connected
  if (Usb.Init() == -1) {    // If USB shield did not initialise for whatever reason...
    while(1); //halt as long as shield is reported disconnected
  }

}


void loop(){
  Usb.Task();
  // Let's process the Xbox input
  if(XboxRCV.Xbox360Connected[0]) {

    // START button sends exitSafeStart command to SMC
    if(XboxRCV.getButtonClick(START,0)){
      // *******************************
      // *        exitSafeStart        *
      // *******************************
      // Required to allow motors connected to SMC to move
      // Must be called when controller restarts and after any error
      // Pololu Protocol: 0xAA (170) | device number | 0x03 (3)
      Serial.write(pololuCommandByte);
      Serial.write(smcDeviceNumber);
      Serial.write(3);
    }

    /* The Xbox triggers provide values from 0 - 255. The SMC will accept a low
     resolution speed value as a percentage, 0% - 100% (High resolution is 
     a 2-byte int). The next two lines maps the controller to output a 
     negative value for L2 (Reverse) and positive for R2 (Forward). These two
     values are then summed to provide the final speed and direction. This is 
     so that both triggers can be held simultaneosly without causing the values 
     to oscillate between Forward and Reverse 
     */
    char XboxL2 = map((XboxRCV.getButtonPress(L2,0)), 0, 255, 0, -100);
    char XboxR2 = map((XboxRCV.getButtonPress(R2,0)), 0, 255, 0, 100);


    // Sum the mapped inputs together to give a final speed and direction
    smcSpeed = XboxL2 + XboxR2;



    /* The sample code for the Xbox controller gave a deadzone of -7500 to 7500.
     This code maintains a dead zone for now but it still needs to be calibrated 
     (I would like to make it adjustable while the sketch is running). */

    leftStickX = map(XboxRCV.getAnalogHat(LeftHatX,0), -32768, 32767, 0, 3880);  // Analog stick moved
    // Set the dead band in the left analog stick. Would like this to be adjustable
    if ((leftStickX >= 1500) && (leftStickX <= 1983)){
      leftStickX = 1400; // The value of leftStickX needs to be a variable 
    }
  }

  // If no triggers/sticks are moving, then center and zero
  else {
    leftStickX = 1400;
    smcSpeed = 0;
  }


  // ************* RESERVED "HEARTBEAT" **********
  // "Heartbeat" will send a serial command every x seconds as a "keep-alive" to the SMC and JRK
  // controllers. It will also prevent duplicate commands from flooding the serial buffer (ideal 
  // for Xbee implementation). 


  // *******************************
  // *      SEND SERIAL COMMANDS   *
  // *******************************
  /* Reserved for serial commands sent to motor controllers to adjust 
   optional parameters. Also to process the response from those 
   commands if applicable. */



  // THIS SECTION SENDS THE SPEED AND DIRECTION COMMANDS TO THE SMC 
  // *******************************
  // *        setMotorSpeed        *
  // *******************************
  /*
  http://www.pololu.com/docs/0J44/6.2.1
   
   The Pololu SMC can use a full resolution speed value (-3200 to 3200), however, this is not needed
   (yet) since the Xbox controller analog triggers only output 0 to 255. The below tables are copied
   straight from the manual linked above. We'll be using a low resolution speed value expressed in
   percentage (0 to 100).
   
   "Alternate Interpretation: The allowed values for the second speed data byte are 0 - 100, 
   so you can ignore the first speed data byte (always set it to 0), and consider the 
   second data byte to simply be the speed percentage. For example, to drive the motor at 
   53% speed, you would use byte1=0 and byte2=53."
   
   Motor Forward
   	                Command Byte	Data Byte 1	Data Byte 2	Data Byte 3	Data Byte 4
   Pololu Alternate Use	0xAA (170)	device number	0x05 (5)	0 (always)      speed %
   
   
   Motor Reverse (data byte 2 changes)
   	                Command Byte	Data Byte 1	Data Byte 2	Data Byte 3	Data Byte 4
   Pololu Alternate Use	0xAA (170)	device number	0x06 (6)	0 (always)      speed %
   
   */
  // smcSpeed should be a number from -100 to 100

    // First send the Pololu SMC command byte
  Serial.write(pololuCommandByte);

  // Next, send the SMC device number
  Serial.write(smcDeviceNumber);

  // Here, let's determine the speed and direction.
  if (smcSpeed < 0)  // Let's reverse since the speed is negative
  {
    Serial.write(6);  // motor reverse command
    smcSpeed = -smcSpeed;  // make smcSpeed positive b/c the Pololu command can only read positive numbers
  }
  else
  {
    Serial.write(5);  // motor forward command
  }

  Serial.write(smcSpeedDataByte3);  // Always zero (for now) because of the protocol being used

  // Now let's send the actual speed
  Serial.write(smcSpeed);
  delay(1);  // For stability, don't know if I need it here or at the end of the whole sketch


  // NEXT SECTION SENDS THE POSITION TO THE LINEAR ACTUATOR VIA THE JRK
  // *******************************
  // *          setJRKPos          *
  // *******************************
  /* http://www.pololu.com/docs/0J38/4.e
   Pololu protocol, hex: 0xAA, device number, 0x40 + target low 5 bits, target high 7 bits
   Here is some example C code that will generate the correct serial bytes, 
   given an integer “target" that holds the desired target (0 - 4095) and an array called serialBytes:
   
   1 serialBytes[0] = 0xC0 + (target & 0x1F); // Command byte holds the lower 5 bits of target.
   2 serialBytes[1] = (target >> 5) & 0x7F;   // Data byte holds the upper 7 bits of target. */

  Serial.write(pololuCommandByte);
  Serial.write(jrkDeviceNumber);
  Serial.write(0x40 + (leftStickX & 0x1F));
  Serial.write((leftStickX >> 5) & 0x7F);

  delay(1);  // For stability


}



