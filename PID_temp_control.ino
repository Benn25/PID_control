/*
You will need the following:
-Arduino (UNO, NANO, Pro Micro...)
-OLED display SSD1306_128X32 IIC
-10K potentiometer
-100K temp sensor + 100K resistor
-a blank PCB
-1 PCB switch
Optional:
-addressible LED strip (led ws2812b is perfect)
-the enclosure files I made (and 3D print it). It fits a Ardiono pro micro.
-the following libraries : PID, Fastled and U8glib
*/


#include <PID_v1.h>
#include <FastLED.h>
#include <SPI.h>
#include <Wire.h>
#include "U8glib.h"


U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);  // I2C / TWI

#define pot A1
#define probe A2
#define min_set_temp 30
#define NUMSAMPLES 40
#define long_press 800

///////////// LED //////////////

#define LED_PIN     12
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    48
#define BRIGH_max 210
int BRIGHTNESS = 0;
int factorbright = 0;
#define FRAMES_PER_SECOND 100
CRGB leds[NUM_LEDS];
int LEDPOS = 0; //LED to be lighten up
int factorpos = 1;
///////// END LED /////////


const int butt = 8; //multi purpose button (press the pot)
const int heat = 9; //relay for the heater
//int targetTemp;
double  targetTemp, temp, Output; //for the PID. temp is from the sensor, targetTemp is mapped from the pot

bool LIGHT = true; //are the LED ON?
bool HEATER = true; //is the heater running?

unsigned long timer; //timer for edit mode
int butTimer; //recording the time the button is pressed (short press or long press?)
int sensor[NUMSAMPLES]; //one record of the temp, to be averaged
byte iter = 0;

//for PID:
double Kp = 50, Ki = 10, Kd = 1;
PID myPID(&temp, &Output, & targetTemp, Kp, Ki, Kd, DIRECT);
int WindowSize = 2000;
unsigned long windowStartTime;

//image edit, size is 15*15
static unsigned char u8g_edit[] U8G_PROGMEM = {
  0x00, 0x18, 0x00, 0x3C, 0x00, 0x78, 0x00, 0x73, 0x80, 0x27, 0xC0, 0x0F,
  0xE0, 0x0F, 0xF0, 0x07, 0xF8, 0x03, 0xFC, 0x01, 0xFE, 0x00, 0x7F, 0x00,
  0x3F, 0x00, 0x1F, 0x00, 0x0F, 0x00,
};

static unsigned char u8g_heat[] U8G_PROGMEM = {
  0x30, 0xC6, 0x00, 0x30, 0xC6, 0x00, 0x10, 0x42, 0x00, 0x18, 0x63, 0x00,
  0x18, 0x63, 0x00, 0x18, 0x63, 0x00, 0x38, 0xE7, 0x00, 0x30, 0xC6, 0x00,
  0x30, 0xC6, 0x00, 0x10, 0x42, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xFF, 0x03,
  0xFF, 0xFF, 0x0F, 0xDB, 0xB6, 0x0D, 0xDB, 0xB6, 0x0D, 0xDB, 0xB6, 0x0D,
  0xDB, 0xB6, 0x0D, 0xDB, 0xB6, 0x0D, 0xFF, 0xFF, 0x0F, 0xFC, 0xFF, 0x03,
};

static unsigned char u8g_noheat[] U8G_PROGMEM = {
  0x30, 0xC6, 0x00, 0x30, 0xC6, 0x08, 0x10, 0x42, 0x0C, 0x18, 0x63, 0x06,
  0x18, 0x23, 0x03, 0x18, 0x83, 0x01, 0x38, 0xC7, 0x00, 0x30, 0x66, 0x00,
  0x30, 0x32, 0x00, 0x10, 0x18, 0x00, 0x00, 0x0C, 0x00, 0x7C, 0xE6, 0x03,
  0x3F, 0xF3, 0x0F, 0x9B, 0xB1, 0x0D, 0xCB, 0xB4, 0x0D, 0x63, 0xB6, 0x0D,
  0x33, 0xB6, 0x0D, 0x99, 0xB6, 0x0D, 0xCC, 0xFF, 0x0F, 0xE6, 0xFF, 0x03,
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(butt, INPUT_PULLUP);
  pinMode(heat, OUTPUT);

  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

  windowStartTime = millis();
  targetTemp = min_set_temp;

  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, WindowSize);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);
  u8g.setRot270();

}

//////////////////////////////////////////////////////////////////////
/////////////////////// FUNCTIONS \\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////

void drawBoot() {

  u8g.drawDisc( 32 / 2, 128 - 14, 12, U8G_DRAW_ALL);
  u8g.drawDisc( 32 / 2, 6, 6, U8G_DRAW_ALL);
  u8g.drawBox( 32 / 2 - 6, 8, 13, 128 - 14 - 5 );
  u8g.setColorIndex(0);
  u8g.drawDisc( 32 / 2, 128 - 14, 8, U8G_DRAW_ALL);
  u8g.drawBox(32 / 2 - 6 + 3, 128 - 14 - 5 - 10 - millis() / 50, 7, 128 - 14 - 5 - 100 + millis() / 50);
  for (int a = 0 ; a < 80 ; a += 5) {
    u8g.drawLine(32 / 2 + 5, a + 20, 32 / 2 + 8, a + 20);
    u8g.drawLine(32 / 2 + 6, a + 20 + 1, 32 / 2 + 8, a + 20 + 1);
  }
  u8g.setColorIndex(1);
 }

void drawTemp() { //draw actual temperature and set temperature
  u8g.setFont(u8g_font_helvB10);
  u8g.setScale2x2();
  if (butTimer == 0) {// no edit

  }

  HEATER == true ?  u8g.setPrintPos(0, 60) : u8g.setPrintPos(0, 28) ;
  u8g.print(round(temp));

  if (HEATER == true) {
    u8g.undoScale();
    u8g.setFont(u8g_font_5x7);
    u8g.setPrintPos(6, 10);
    u8g.print("set:");
    u8g.setPrintPos(0, 91);
    u8g.print("actual:");

    u8g.setFont(u8g_font_helvB10);
    u8g.setScale2x2();
    u8g.setPrintPos(0, 19);
    u8g.print(round(targetTemp));
    u8g.undoScale();
    if (butTimer == 0) {
      u8g.drawXBMP( 5, 52, 20, 20, u8g_heat);
      if (digitalRead(heat) == 0) { //when heater off state during the PID window
        u8g.setColorIndex(0);
        u8g.drawBox(1, 50, 27, 13);
        u8g.setColorIndex(1);
      }
    }
  }
  else { //when no heater
    u8g.undoScale();
    u8g.drawXBMP( 4, 82, 20, 20, u8g_noheat);
  }
  u8g.undoScale();
  if (butTimer > 0 && HEATER == true ) { //when in editing mode
    u8g.drawXBMP( 9, 53, 15, 15, u8g_edit);
  }
}

void drawMenu() { //draw the menu to toggle light and/or heater
  //u8g.setFont(u8g_font_helvB08);
  //u8g.setFont(u8g_font_04b_03r);
  u8g.setFont(u8g_font_5x7);
  u8g.setPrintPos(0, 20);
  u8g.print("toggle:");
  u8g.setPrintPos(5, 50);
  u8g.print("LED");
  u8g.setPrintPos(5, 65);
  u8g.print("Heater");
  u8g.setPrintPos(5, 80);
  u8g.print("ALL");
  u8g.setPrintPos(5, 105);
  u8g.print("back");

  if (analogRead(pot) < 250) {
    u8g.setPrintPos(0, 50);
    u8g.print(">");
    Serial.println("tg LED");
  }
  if (analogRead(pot) >= 255 && analogRead(pot) < 500) {
    u8g.setPrintPos(0, 65);
    u8g.print(">");
    Serial.println("tg heat");
  }
  if (analogRead(pot) >= 500 && analogRead(pot) < 750) {
    u8g.setPrintPos(0, 80);
    u8g.print(">");
  }
  if (analogRead(pot) >= 750) {
    u8g.setPrintPos(0, 105);
    u8g.print(">");
  }

}

void fadeall() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // leds[i].nscale8(254);
    leds[i].nscale8_video(252);
    //  qsub8(200,50);
    //fadeUsingColor(40,100,80);
    //blend( leds[i], (40,100,80),252);
    //TGradientDirectionCode directionCode = SHORTEST_HUES );

  }
  //fadeUsingColor(50,5,0);
}


//////////////////////////////////////////////////////////////////////
///////////////////////////// LOOP \\\\\\\\\\\\\\\\\\\\\\\\\\\\
//////////////////////////////////////////////////////////////////////

void loop() {
  butTimer = 0;

  sensor[iter] = analogRead(probe);
  //I tried a lib for temp sensor, but it wasn't working,
  //so I mesured the raw values of the sensor at severals temps
  //and I created a function to approximate the curve
  //it works well enough, but not very precise under 35C
  //function temperature temp=35.4- 0.105*x + 0.000167*x*x            x is the value from the sensor
  
  sensor[iter] = 35.4 - 0.106 * sensor[iter] + 0.00017 * sensor[iter] * sensor[iter];
  iter >= NUMSAMPLES - 1 ? iter = 0 : iter++;

  temp = 0;
  for (int i = 0 ; i < NUMSAMPLES ; i++) {
    temp += sensor[i];
  }
  temp /= NUMSAMPLES;
  //  delay(100);

  ////////////////////// PID COMPUTE ///////////////////////
  if (HEATER == true) {
    myPID.Compute();
    if (millis() - windowStartTime > WindowSize) {
      //time to shift the Relay Window
      windowStartTime += WindowSize;
    }
    if (Output < millis() - windowStartTime) {
      digitalWrite(heat, LOW);
    }
    else digitalWrite(heat, HIGH);
  }
  if (HEATER == false) {
    digitalWrite(heat, LOW);
  }
  ////////////////////// END PID COMPUTE ///////////////////////

  while (digitalRead(butt) == 0) { //buton pressed, recording time
    digitalWrite(heat, LOW); //disconnect the heater as a security mesure.
    butTimer++;
    Serial.println(butTimer);
  }
  if (butTimer > 25 && butTimer < long_press) { // MODIFIER for button short pressed during running mode
    //EDIT = true;
    //timer = millis(); //rester the timer
    //butTimer = 0; //reset the button timer
    delay(70);

    while (butTimer > 0 && digitalRead(butt) == 1 && HEATER == true) { //we are editing the target tempertature
      targetTemp =  map(analogRead(pot), 0, 1022, 90, min_set_temp);
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i]  = CHSV(110 - targetTemp, 160, 200);
      }
      FastLED.show(); // display this frame

      u8g.firstPage();
      do {
        drawTemp();
      }
      while ( u8g.nextPage() );

      Serial.println("EDIT");
      Serial.print("   target = ");
      Serial.println( targetTemp);

      if (digitalRead(butt) == 0) {
        delay(40);
        Serial.println("tamere");
        butTimer = 0;
        while (digitalRead(butt) == 0) {
          delay(25);
        }
      }
    }

  }
  if (butTimer >= long_press) { // MODIFIER for button longt pressed during running mode
    //go to menu page for toggle light / heat
    while (butTimer > 0 && digitalRead(butt) == 1) {
      u8g.firstPage();
      do {
        drawMenu();
      }
      while ( u8g.nextPage() );

      if (digitalRead(butt) == 0) {
        delay(40);

        if (analogRead(pot) < 250) {
          Serial.println("tg LED");
          LIGHT = !LIGHT;
        }
        if (analogRead(pot) >= 255 && analogRead(pot) < 500) {
          Serial.println("tg heat");
          HEATER = !HEATER;
        }
        if (analogRead(pot) >= 505 && analogRead(pot) < 750) {
          //  Serial.println("tg all");
          HEATER = !HEATER;
          LIGHT = !LIGHT;
        }
        if (analogRead(pot) >= 755 && analogRead(pot) <= 1023) {
          //  Serial.println("back");
        }

        butTimer = 0;
        delay(800);
        while (digitalRead(butt) == 0) {
          delay(25);
        }
      }
    }
  }


  Serial.println(digitalRead(heat));
  Serial.println(round(temp));
  Serial.print("   target = ");
  Serial.println( round(targetTemp));
  Serial.println("----------------");

  if (millis() < 4000) {
    u8g.firstPage();
    do {
      drawBoot();
    }
    while ( u8g.nextPage() );
  }

  else {
    u8g.firstPage();
    do {
      drawTemp();
    }
    while ( u8g.nextPage() );
  }

  //here the code for PID modulation
  // u8g.setFont(u8g_font_helvB08);
  //u8g.setFont(u8g_font_unifont);
  //u8g.setFont(u8g_font_osb21);
  //u8g.setFont(u8g_font_6x13);
  //u8g.setFontRefHeightText();
  //u8g.setFontPosTop();

  if (HEATER == true) { //heater ON
    Serial.println("HEATER ON");
  }
  else {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i]  = CHSV(160, 100, 150);
    }
  }

  if (LIGHT == true) { //light ON
    Serial.println("light are ON");
    if (BRIGHTNESS < BRIGH_max) {
      factorbright += 4;
      BRIGHTNESS = factorbright;
      FastLED.setBrightness( BRIGHTNESS );
    }

    if (HEATER == true) {
      factorpos / 2 < NUM_LEDS - 1 ? factorpos++ : factorpos = 1;
      LEDPOS = factorpos / 2;
      leds[LEDPOS]  = CHSV(110 - targetTemp, 150, 200);
      Serial.println (LEDPOS);
    }

  } //light off
  else {
    if (BRIGHTNESS > 0) {
      factorbright -= 4;
      BRIGHTNESS = factorbright;
      Serial.println (BRIGHTNESS);
      FastLED.setBrightness( BRIGHTNESS );
    }
  }
  fadeall();

  FastLED.show(); // display this frame
}
