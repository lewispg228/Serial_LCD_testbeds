/*
  SerLCD testbed code.
  Flying Jalapeno (VCC (aka LOGIC): 3.3V, V1:3.3V,  V2: RAW (5-7V))
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
#define CS_PIN 53

// Display size. The same testbed code goes onto both versions of the testing hardware,
// and the following two definitions change the messages sent during testing.
// Define one of these two following variables as a "1" and the other as a "0".
#define DISPLAY_SIZE_16X2 0
#define DISPLAY_SIZE_20X4 1

#define DISPLAY_ADDRESS1 0x72 //This is the default address of the Serial1

#include <FlyingJalapeno.h>
FlyingJalapeno FJ(STATUS_LED, 3.3); //Blink status msgs on pin 13

#include <Wire.h>

#include <SPI.h>

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
        FJ.setV1(true, 3.3); //Turn on power supply 1 to 3.3V
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
    test_VCC();
    //contrast_test(); // just here temporarily to make sure this is working      
    set_contrast_via_serial(10);
    serial_test();
    //backlight_test_loop();
    delay(200);
    I2C_test();
    delay(200);
    if(failures == 0) SPI_test();
    delay(200);
    if(failures == 0)
    {
      backlight_rgb_upfades(500);
    }
}

// This is an example of testing a 3.3V output from the board sent to A2.
// This was originally used on the ESP32 dev board testbed.
void test_VCC()
{
  Serial.println("testing 3.3V output on board VCC");

  // THIS IS SPLIT WITH A PRETEST CIRCUIT
  // This means that we need to write the PT_CTRL pin LOW, in order to get a split "closer" to 50%.
  pinMode(PT_CTRL_PIN, OUTPUT);
  digitalWrite(PT_CTRL_PIN, LOW);
    
  //pin = pin to test
  //correct_val = what we expect.
  //allowance_percent = allowed window for overage. 0.1 = 10%
  //debug = print debug statements
  boolean result = FJ.verifyVoltage(A6, 1.7, 10, true); // 3.3V split by two 10Ks in the PT circuit

  if (result == true) 
    Serial.println("test success!");
  else
  {
    Serial.println("test failure (should read near 1.7 V)");
    failures++;
  }
}

void power_down()
{
  Serial.println("powering down target");

  Serial1.end();
  Wire.end();
  SPI.end();
  digitalWrite(CS_PIN, LOW);

  FJ.disableRegulator1();
  FJ.disableRegulator2();

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
  Serial1.begin(9600);
  while(1)
  {
    Serial.print("Contrast: ");
    Serial.println(contrast);
    set_contrast_via_serial(contrast);
    
    Serial1.write('|'); //Setting character
    Serial1.write('-'); //Clear display
    Serial1.print("Contrast: ");
    Serial1.print(contrast);
    
    contrast += 10;
    delay(2000); //Hang out for a bit
  }
}

void set_contrast_via_serial(int contrast)
{
  Serial1.begin(9600);

    // Note, this is only here because I can't get the RED or BLUE backlight to work right now, and I want to test com lines.
    Serial.println("Green brightness: 29"); // debug
    Serial1.write('|'); //Put LCD into setting mode
    Serial1.write(158 + 29); //Set green backlight amount to 0%
    delay(100);
  
  //Send contrast setting
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(24); //Send contrast command (24 aka "ctrl-x")
  Serial1.write(contrast); // "10" contrast setting (0-255) seems to look perfect on my test jig in my office.
  delay(100);
}

void serial_test()
{
  Serial1.write('|'); //Setting character
  Serial1.write('-'); //Clear display
  if(DISPLAY_SIZE_20X4) Serial1.print("Serial              ");
  else if(DISPLAY_SIZE_16X2) Serial1.print("Serial          "); // shorter amount of "spaces" - makes total length of characters 16
  delay(100);
}

boolean ping_I2C()
{
  #include <Wire.h>
  Wire.begin();
  boolean result = FJ.verify_i2c_device(DISPLAY_ADDRESS1);
  if(result == false) failures++;
  return result;
}

void I2C_test()
{
  FJ.enablePCA();
  if(ping_I2C() == true)
  {
    Wire.beginTransmission(DISPLAY_ADDRESS1); // transmit to device #1
    if(DISPLAY_SIZE_20X4) Wire.print("I2C                 ");
    else if(DISPLAY_SIZE_16X2) Wire.print("I2C          "); // shorter amount of "spaces" - makes total length of characters 13 - note, we need room for "SPI"
    Wire.endTransmission(); //Stop I2C transmission
  }
  else
  {
    Serial.print("I2C ping FAILURE");
  }
  Wire.end();
  FJ.disablePCA();
}

void SPI_test()
{
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH); //By default, don't be selecting Serial1
  
  SPI.begin(); //Start SPI communication
  //SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE0));
  SPI.setClockDivider(SPI_CLOCK_DIV64); //Slow down the master a bit

    //Send the clear display command to the display - this forces the cursor to return to the beginning of the display
  //digitalWrite(CS_PIN, LOW); //Drive the CS pin low to select Serial1
  //SPI.transfer('|'); //Put LCD into setting mode
  //SPI.transfer('-'); //Send clear display command
  //digitalWrite(CS_PIN, HIGH); //Release the CS pin to de-select Serial1

  char tempString[50]; //Needs to be large enough to hold the entire string with up to 5 digits
  if(DISPLAY_SIZE_20X4) sprintf(tempString, "SPI                 ");
  else if(DISPLAY_SIZE_16X2) sprintf(tempString, "SPI"); // just fit in in the last 3 spots in the bottom row. The total row will display "I2C          SPI"
  spiSendString(tempString);

}

//Sends a string over SPI
void spiSendString(char* data)
{
  digitalWrite(CS_PIN, LOW); //Drive the CS pin low to select Serial1
  for(byte x = 0 ; data[x] != '\0' ; x++) //Send chars until we hit the end of the string
    SPI.transfer(data[x]);
  digitalWrite(CS_PIN, HIGH); //Release the CS pin to de-select Serial1
}

void backlight_test_RGB(int delay_var)
{

  //Serial1.begin(9600); //Begin communication with Serial1

  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(158 + 0); //Set green backlight amount to 0%
  delay(100);

  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(188 + 0); //Set blue backlight amount to 0%
  delay(100);

  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128); //Set white/red backlight amount to 0%
  delay(100);

  ///////////////////////RED
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128 + 29); //Set white/red backlight amount to 100%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128); //Set white/red backlight amount to 0%
  
  ///////////////////////GREEN
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(158 + 29); //Set green backlight amount to 100%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(158 + 0); //Set green backlight amount to 0%

  ///////////////////////BLUE
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(188 + 29); //Set blue backlight amount to 100%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(188 + 0); //Set blue backlight amount to 0%

  
  ///////////////////////WHITE
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128 + 29); //Set white/red backlight amount to 100%  
  Serial1.write(158 + 29); //Set green backlight amount to 100%  
  Serial1.write(188 + 29); //Set blue backlight amount to 100%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128); //Set white/red backlight amount to 0%  
  Serial1.write(158 + 0); //Set green backlight amount to 0%  
  Serial1.write(188 + 0); //Set blue backlight amount to 0%
  
}

void backlight_rgb_upfades(int delay_var)
{
  int brightness[3] = {0,15,29};
  //Serial1.begin(9600); //Begin communication with Serial1

  // ALL OFF

  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(158 + 0); //Set green backlight amount to 0%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(188 + 0); //Set blue backlight amount to 0%
  delay(delay_var);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128); //Set white/red backlight amount to 0%
  delay(delay_var);

  // RED UPFADE
    for( int i = 0 ; i <= 2 ; i++)
    {
      Serial.print("brightness: "); // debug
      Serial.println(brightness[i]); // debug
      Serial1.write('|'); //Put LCD into setting mode
      Serial1.write(128 + brightness[i]); //Set white/red backlight amount to 0%
      delay(delay_var);
    }
    Serial1.write('|'); //Put LCD into setting mode
    Serial1.write(128); //Set white/red backlight amount to 0%
    delay(delay_var);    

  // GREEN UPFADE
  for( int i = 0 ; i <= 2 ; i++)
    {
      Serial.print("brightness: "); // debug
      Serial.println(brightness[i]); // debug
      Serial1.write('|'); //Put LCD into setting mode
      Serial1.write(158 + brightness[i]); //Set green backlight amount to 0%
      delay(delay_var);
    }    
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(158 + 0); //Set green backlight amount to 0%
  delay(delay_var);    

  // BLUE UPFADE
  for( int i = 0 ; i <= 2 ; i++)
    {
      ///////////////////////RED
      Serial.print("brightness: "); // debug
      Serial.println(brightness[i]); // debug
      Serial1.write('|'); //Put LCD into setting mode
      Serial1.write(188 + brightness[i]); //Set blue backlight amount to 0%
      delay(delay_var);
    }        
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(188 + 0); //Set blue backlight amount to 0%
  delay(delay_var); 
}

void backlight_test_monochrome()
{

  //Serial1.begin(9600); //Begin communication with Serial1

  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128 + 29); //Set white/red backlight amount to 0%
  delay(2000);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128 + 15); //Set white/red backlight amount to 51%
  delay(2000);
  Serial1.write('|'); //Put LCD into setting mode
  Serial1.write(128); //Set white/red backlight amount to 100%

}

// Note, this is only here because I can't get the RED or BLUE backlight to work right now, and I want to test com lines.
void backlight_test_loop()
{
  int BL128 = 0;
  int BL158 = 0;
  int BL188 = 0;
  
  while(1)
  {

  
  Serial1.begin(9600);
  
  if(Serial.available() > 0)
  {
    char input = Serial.read();
    switch (input) 
    {
      case '1': 
        BL128 -= 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(128 + BL128); //Set green backlight amount to 0%
        delay(100);        
        break;
      case '4':
        BL128 += 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(128 + BL128); //Set green backlight amount to 0%
        delay(100);
        break;
      case '2': 
        BL158 -= 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(158 + BL158); //Set green backlight amount to 0%
        delay(100);        
        break;
      case '5':
        BL158 += 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(158 + BL158); //Set green backlight amount to 0%
        delay(100);        
        break;
      case '3': 
        BL188 -= 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(188 + BL188); //Set green backlight amount to 0%   
        delay(100);        
        break;
      case '6':
        BL188 += 1;
        Serial1.write('|'); //Put LCD into setting mode
        Serial1.write(188 + BL188); //Set green backlight amount to 0%   
        delay(100);        
        break;
    }

    Serial.println(BL128);
    Serial.println(BL158);
    Serial.println(BL188);

  }
  }
}

