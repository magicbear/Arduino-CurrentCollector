#define CALC_DC_RMS
//#define PC_OSC_DEBUG
//#define MAX72XX_OUTPUT

#include "math.h"
#include <SPI.h>

#ifdef MAX72XX_OUTPUT
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

int pinCS = 10; // Attach CS to this pin, DIN to MOSI and CLK to SCK (cf http://arduino.cc/en/Reference/SPI )
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays = 1;
#define _DISPLAY_ROTATE 1

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

int i = 0;
int spacer = 1;
int width = 5 + spacer; // The font width is 5 pixels

// 3x5 char maps
uint16_t numMap3x5[] = {
  32319,  //0
  10209,  //1
  24253, //2
  22207,  //3
  28831,  //4
  30391,  //5
  32439,  //6
  16927,  //7
  32447,  //8
  30399  //9
};
// 4x5 char maps
uint32_t numMap4x5[] = {
  476718,  //0
  10209,  //1
  315049, //2
  579246,  //3
  478178,  //4
  972470,  //5
  480418,  //6
  544408,  //7
  349866,  //8
  415406  //9
};

String tape = "";
#endif

#define ADC_BASE_VOLTAGE 3.3
#define OP_A1            (0.019 / 0.9)
#define OP_A2            (0.019 / 0.9)
#define OP_A3            (0.019 / 0.9)

#ifdef CALC_DC_RMS
long rmsA1 = 0;
float rmsA2 = 0;
float rmsA3 = 0;
#endif
long rmsAC_A1 = 0;
long rmsAC_A2 = 0;
long rmsAC_A3 = 0;
long peakA1 = 0;
long peakA2 = 0;
long peakA3 = 0;
long lastVolA1 = 0;
long lastVolA2 = 0;
long lastVolA3 = 0;
int totalCnt = 0;
int   countHZ_A1 = 0;
int   countHZ_A2 = 0;
int   countHZ_A3 = 0;
unsigned long time = 0;
unsigned long execMills = 0;
char buffer[256];

unsigned char clockPin = 0;

int pp_A1 = -1;
int hp_A1 = 0;
int lp_A1 = 1024;

int pp_A2 = -1;
int hp_A2 = 0;
int lp_A2 = 1024;

int pp_A3 = -1;
int hp_A3 = 0;
int lp_A3 = 1024;

int secondPass = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
//  Serial.println("Startup...");
  pinMode(7, OUTPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  
#ifdef MAX72XX_OUTPUT
  matrix.setIntensity(2); // Set brightness between 0 and 15
  for (int i = 0; i< numberOfHorizontalDisplays;  i++)
    matrix.setRotation(i,_DISPLAY_ROTATE);
#endif

}

#ifdef MAX72XX_OUTPUT
void drawMapValue3x5(Max72xxPanel display, uint8_t x, uint8_t y, uint32_t val)
{
  for (uint8_t i = 0; i < 20; i++)
  {
    if ((val >> i) & 1 == 1) {
      display.drawPixel(x + (3 - i / 5) - 1, y + (4 - i % 5), HIGH);
    }
  }
}

void drawMapValue4x5(Max72xxPanel display, uint8_t x, uint8_t y, uint32_t val)
{
  for (uint8_t i = 0; i < 20; i++)
  {
    if ((val >> i) & 1 == 1) {
      display.drawPixel(x + (4 - i / 5) - 1, y + (4 - i % 5), HIGH);
    }
  }
}
#endif

void loop() {
  // put your main code here, to run repeatedly:
  // 221us = 0.3ms
  
  long a1 = analogRead(A1);
  long a2 = analogRead(A2);
  long a3 = analogRead(A3);

#ifdef PC_OSC_DEBUG
  sprintf(buffer, "S:%ld,%ld,%ld:D", a1, a2, a3);
  Serial.println(buffer);
#endif

  if (a1 > hp_A1) {
    hp_A1 = a1;
    pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2;
  }
  if (a1 < lp_A1) {
    lp_A1 = a1;
    pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2;
  }
  
  if (a2 > hp_A2) {
    hp_A2 = a2;
    pp_A2 = lp_A2 + (hp_A2 - lp_A2) / 2;
  }
  if (a2 < lp_A2) {
    lp_A2 = a2;
    pp_A2 = lp_A2 + (hp_A2 - lp_A2) / 2;
  }
  
  if (a3 > hp_A3) {
    hp_A3 = a3;
    pp_A3 = lp_A3 + (hp_A3 - lp_A3) / 2;
  }
  if (a3 < lp_A3) {
    lp_A3 = a3;
    pp_A3 = lp_A3 + (hp_A3 - lp_A3) / 2;
  }
  
  if (a1 - pp_A1 <= 0 && lastVolA1 - pp_A1 > 0)
  {
    rmsAC_A1 += peakA1 * peakA1;
    // full cycle of Sin Waves
    peakA1 = 0;
    countHZ_A1 = countHZ_A1 + 1;
  }

  if (a2 - pp_A2 <= 0 && lastVolA2 - pp_A2 > 0)
  {
    rmsAC_A2 += peakA2 * peakA2;
    // full cycle of Sin Waves
    peakA2 = 0;
    countHZ_A2 = countHZ_A2 + 1;
  }

  if (a3 - pp_A3 <= 0 && lastVolA3 - pp_A3 > 0)
  {
    rmsAC_A3 += peakA3 * peakA3;
    // full cycle of Sin Waves
    peakA3 = 0;
    countHZ_A3 = countHZ_A3 + 1;
  }

  lastVolA1 = a1;
  lastVolA2 = a2;
  lastVolA3 = a3;
  
#ifdef CALC_DC_RMS
  rmsA1 += a1 * a1;
  rmsA2 += a2 * a2;
  rmsA3 += a3 * a3;
#endif

  if (a1 - pp_A1 > peakA1) peakA1 = a1 - pp_A1;
  if (a2 - pp_A2 > peakA2) peakA2 = a2 - pp_A2;
  if (a3 - pp_A3 > peakA3) peakA3 = a3 - pp_A3;

  totalCnt++;

  // 816us
  clockPin = !clockPin;
//  digitalWrite(7, clockPin);
//  PIND = clockPin;
  if (millis() - execMills >= 1000)
  {
      secondPass = secondPass + millis() - execMills;
      execMills = millis();

      if (countHZ_A1 == 0) countHZ_A1 = 1;
      if (countHZ_A2 == 0) countHZ_A2 = 1;
      if (countHZ_A3 == 0) countHZ_A3 = 1;
      
#ifdef MAX72XX_OUTPUT
      dtostrf(sqrt(double(rmsAC_A1) / countHZ_A1)/1024.*ADC_BASE_VOLTAGE * 0.707 / OP_A1, 1, 4, buffer);
      tape = buffer;
#endif

      char *p = buffer + sprintf(buffer, "Samples: %d  HZ: %d/%d/%d", totalCnt, countHZ_A1, countHZ_A2, countHZ_A3);
//      p+= sprintf(p, " P-P:");
//      p = dtostrf(hp_A1, 1, 3, p) + 4;
//      p+= sprintf(p, " ");
//      p = dtostrf(pp_A1, 1, 3, p) + 4;
//      p+= sprintf(p, " ");
//      p = dtostrf(lp_A1, 1, 3, p) + 4;
#ifdef CALC_DC_RMS
      p+= sprintf(p, " DC RMS: ");
      p = dtostrf(sqrt(double(rmsA1) / totalCnt)/1024.*ADC_BASE_VOLTAGE, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(sqrt(double(rmsA2) / totalCnt)/1024.*ADC_BASE_VOLTAGE, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(sqrt(double(rmsA3) / totalCnt)/1024.*ADC_BASE_VOLTAGE, 1, 4, p) + 5;
#endif
      float rms_a1 = sqrt(double(rmsAC_A1) / countHZ_A1)/1024.*ADC_BASE_VOLTAGE * 0.707;
      float rms_a2 = sqrt(double(rmsAC_A2) / countHZ_A2)/1024.*ADC_BASE_VOLTAGE * 0.707;
      float rms_a3 = sqrt(double(rmsAC_A3) / countHZ_A3)/1024.*ADC_BASE_VOLTAGE * 0.707;
      p+= sprintf(p, " AC RMS: ");
      p = dtostrf(rms_a1, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(rms_a2, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(rms_a3, 1, 4, p) + 5;
      p+= sprintf(p, " AC Amps: ");
      p = dtostrf(rms_a1 / OP_A1, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(rms_a2 / OP_A2, 1, 4, p) + 5;
      p+= sprintf(p, "/");
      p = dtostrf(rms_a3 / OP_A3, 1, 4, p) + 5;
      
      Serial.println(buffer);
#ifdef CALC_DC_RMS
      rmsA1 = 0;
      rmsA2 = 0;
      rmsA3 = 0;
#endif
      peakA1 = 0;
      peakA2 = 0;
      peakA3 = 0;
      rmsAC_A1 = 0;
      rmsAC_A2 = 0;
      rmsAC_A3 = 0;
      totalCnt = 0;
      countHZ_A1 = 0;
      countHZ_A2 = 0;
      countHZ_A3 = 0;
      if (secondPass >= 10)
      {
        secondPass = 0;
        hp_A1 = 0;
        lp_A1 = 5000;
        hp_A2 = 0;
        lp_A2 = 5000;
        hp_A3 = 0;
        lp_A3 = 5000;
      }
#ifdef MAX72XX_OUTPUT
      matrix.fillScreen(LOW);
      int x = 0;
      for (i=0;i<tape.length();i++) {
        if (tape[i] >= '0' && tape[i] <= '9')
        {
          drawMapValue4x5(matrix, x, 0, numMap4x5[tape[i] - '0']);          
          x += 5;
        } else if (tape[i] == '.') {
          matrix.drawPixel(x, 4, HIGH);          
           x += 2;
        } else if (tape[i] == ' ') {
           x += 1;
        } else {
          matrix.drawChar(x, 0, tape[i], HIGH, LOW, 1);
          x+=5;
        }
      }
      matrix.write(); // Send bitmap to display
#endif
  }
}
