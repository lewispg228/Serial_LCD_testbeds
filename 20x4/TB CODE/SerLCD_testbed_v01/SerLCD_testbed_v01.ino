/*
  SerLCD testbed code.
  Flying Jalapeno (V1:5V,  V2: RAW (7V))
  2 capsense pads: Pretest & Power, TEST

  Select Mega2560 from the boards list

  Test procedure:
  
* USER PRESS "PRETEST & POWER" BUTTON
* pretest for shorts to GND on VIN and 3.3V rails
* power up with V2 (jumpered to RAW 7V input source)
* power up V1 (used to power the programming lines switch IC)
* 
* USER ENGAGE PROGRAMMING (via batch file or stand alone Pi_grammer)
* Load bootloader/firmware combined hex
* 
* USER VERIFY SPLASH SCREEN
* 
* USER PRESS "TEST" BUTTON
* Test voltage output is 3.3V
* Set contrast via serial
* Test Serial
* Test I2C
* Test SPI
* Test backlight(s) via Serial
* Leave LCD in ideal user state (contrast set, backlight on)
* 
* 
*/
#define STATUS_LED 13
#define PGM_SWITCH_EN 30
#define PT_CTRL_PIN 3
#define PT_READ_PIN A6
#define DTR_FJ 2

#include <FlyingJalapeno.h>
FlyingJalapeno FJ(STATUS_LED); //Blink status msgs on pin 13

#include <CapacitiveSensor.h>
CapacitiveSensor cs_1 = CapacitiveSensor(47, 45);
CapacitiveSensor cs_2 = CapacitiveSensor(31, 46);

int failures = 0; //Number of failures by the main test routine

boolean targetPowered = false; //Keeps track of whether power supplies are energized

long preTestButton = 0; //Cap sense values for two main test buttons
long testButton = 0;

void setup()
{
  pinMode(STATUS_LED, OUTPUT);
  pinMode(PT_CTRL_PIN, OUTPUT);
  digitalWrite(PT_CTRL_PIN, LOW);

  pinMode(DTR_FJ, INPUT);
  pinMode(18, INPUT); // TX1, connected to RXI on product
  pinMode(19, INPUT); // RX1, connected to TXO on product
  

  FJ.enablePCA(); //Enable the I2C buffer

  Serial.begin(9600);
  Serial.println("Serial Enabled LCD Testbed\n\r");
  
}

void loop()
{
  preTestButton = cs_1.capacitiveSensor(30);
  testButton = cs_2.capacitiveSensor(30);

  //Serial.print(preTestButton);
  //Serial.print("\t");
  //Serial.println(testButton);

  //Is user pressing PreTest button?
  if (preTestButton > 5000)
  {
    FJ.dot(); //Blink status LED to indicate button press

    if (targetPowered == true) 
    {
      power_down(); //Power down the test jig
    }
    else
    {
      //Check v2 for shorts to ground. Note, V1 is being used to power the switching programming lines IC
      // also a third pretest for 3.3V output
      boolean PT_VIN = FJ.powerTest(2);
      boolean PT_33V = FJ.PreTest_Custom(PT_CTRL_PIN, PT_READ_PIN);
      if (PT_VIN == true && PT_33V == true)
      {
        FJ.setV2(true, 5); // note, this is connected to RAW source via jumper on FJ

        // supply 5V to programming lines swittcher IC. Note, this is the only portion of the tested using this power supply, so we can just leave it on 100% of the time.
        FJ.setV1(true, 5); //Turn on power supply 1 to 5.0V
        pinMode(PGM_SWITCH_EN, OUTPUT);
        digitalWrite(PGM_SWITCH_EN, HIGH); // enable switch for initial programming. We will want to disable this when we move to other testing.

        Serial.println("Pre-test PASS, powering up...\n\r");

        targetPowered = true;

        digitalWrite(LED_PT_PASS, HIGH);
        digitalWrite(LED_PT_FAIL, LOW);

        delay(500); // debounce touching
      }
      else
      {
        //Power supply test failed
        failures++;

        FJ.setV2(false, 5); //Turn off power supply 2

        if (PT_VIN == false) Serial.println("Jumper on Power Input Rail (FJ V2, aka board VIN)\n\r");

        if (PT_33V == false) Serial.println("Jumper on 3.3V Rail (board VCC)\n\r");
        
        targetPowered = false;
        
        digitalWrite(LED_PT_FAIL, HIGH);
        digitalWrite(LED_PT_PASS, LOW);

        delay(500); // debounce touching
      }
    }
  }
  else if (testButton > 5000 && targetPowered == true)
  {
    //Begin main test
    
    FJ.dot();

    digitalWrite(PGM_SWITCH_EN, LOW); // DISable switch for programming.
    pinMode(PGM_SWITCH_EN, INPUT);

    failures = 0; // reset for testing a second time

    digitalWrite(LED_PASS, LOW);
    digitalWrite(LED_FAIL, LOW);

    test_VCC();
    //contrast_test();
    test(); //Run main test code

    if (failures == 0)
    {
      digitalWrite(LED_FAIL, HIGH); // accidentally swapped PASS and FAIL LEDs on hardware design
    }
    else
    {
      digitalWrite(LED_PASS, HIGH); // accidentally swapped PASS and FAIL LEDs on hardware design
    }
  }
}

void test()
{
  // add in your test code here

    Serial1.begin(9600);

    //Send contrast setting
    Serial1.write('|'); //Put LCD into setting mode
    Serial1.write(24); //Send contrast command (24 aka "ctrl-x")
    Serial1.write(50); // "50" contrast setting (0-255) seems to look perfect on my test jig in my office.
    delay(500);

    Serial1.write('|'); //Setting character
    Serial1.write('-'); //Clear display
    Serial1.print("01234567890123456789"); // fill all rows.
    Serial1.print("01234567890123456789");
    Serial1.print("01234567890123456789");
    Serial1.print("01234567890123456789");
    
}

// This is an example of testing a 3.3V output from the board sent to A2.
// This was originally used on the ESP32 dev board testbed.
void test_VCC()
{
  Serial.println("testing 5V output on board VCC");

  // THIS IS SPLIT WITH A PRETEST CIRCUIT
  // This means that we need to write the PT_CTRL pin LOW, in order to get a split "closer" to 50%.
  pinMode(PT_CTRL_PIN, OUTPUT);
  digitalWrite(PT_CTRL_PIN, LOW);
    
  //pin = pin to test
  //correct_val = what we expect.
  //allowance_percent = allowed window for overage. 0.1 = 10%
  //debug = print debug statements
  boolean result = FJ.verifyVoltage(A6, 2.4, 10, true); // 5V split by two 10Ks in the PT circuit, reads 486 on my 5V logic FJ (using a proto known good).

  if (result == true) 
    Serial.println("test success!");
  else
  {
    Serial.println("test failure (should read near 2.4 V)");
    failures++;
  }
}

void power_down()
{
  Serial.println("powering down target");

  FJ.setV1(false, 5); //Turn off power supply 1, but leave voltage selection at 5V
  FJ.setV2(false, 4.2); //Turn off power supply 1, but leave voltage selection at 4.2V

  digitalWrite(PGM_SWITCH_EN, LOW); // DISable switch for programming.
  pinMode(PGM_SWITCH_EN, INPUT);
  
  targetPowered = false;

  //Turn off all LEDs
  digitalWrite(LED_PT_PASS, LOW);
  digitalWrite(LED_PT_FAIL, LOW);
  digitalWrite(LED_PASS, LOW);
  digitalWrite(LED_FAIL, LOW);

  failures = 0;

  delay(500); // debounce touching
}

void contrast_test()
{
  byte contrast = 0; //Will roll over at 255
//  Serial1.begin(9600);

  while(1)
  {
    Serial.print("Contrast: ");
    Serial.println(contrast);
    
    //Send contrast setting
    Serial1.write('|'); //Put LCD into setting mode
    Serial1.write(24); //Send contrast command
    Serial1.write(contrast);
    delay(10);
  
    Serial1.write('|'); //Setting character
    Serial1.write('-'); //Clear display
    Serial1.print("Contrast: ");
    Serial1.print(contrast);
  
    contrast += 5;
  
    delay(500); //Hang out for a bit
  }
  
}

