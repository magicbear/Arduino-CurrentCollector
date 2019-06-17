#include "math.h"
#include <SPI.h>
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

#define ADC_BASE_VOLTAGE 3.3
#define OP_A1            (0.42 / 4.29)

float totalVoltageA1 = 0;
float rmsA1 = 0;
float rmsAC_A1 = 0;
float peakA1 = 0;
float lastVolA1 = 0;
int totalCnt = 0;
int   countHZ = 0;
unsigned long time = 0;
unsigned long execMills = 0;
char buffer[256];

unsigned char clockPin = 0;

float pp_A1 = -1;
float hp_A1 = 0;
float lp_A1 = 5;

int secondPass = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Startup...");
  pinMode(7, OUTPUT);
  
  matrix.setIntensity(2); // Set brightness between 0 and 15
  for (int i = 0; i< numberOfHorizontalDisplays;  i++)
    matrix.setRotation(i,_DISPLAY_ROTATE);
//  matrix.setRotation(1,_DISPLAY_ROTATE);
//  matrix.setRotation(2,_DISPLAY_ROTATE);
//  matrix.setRotation(3,_DISPLAY_ROTATE);
}

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

void loop() {
  // put your main code here, to run repeatedly:
  // 221us = 0.3ms
  
  int a1 = analogRead(A1);
  int a2 = analogRead(A2);
  int a3 = analogRead(A3);

  // 2.3475 ms = 2.6475ms
  float volA1 = a1/1024.*ADC_BASE_VOLTAGE;
  
  if (volA1 > hp_A1) {
    hp_A1 = volA1;
    pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2;
  }
  if (volA1 < lp_A1) {
    lp_A1 = volA1;
    pp_A1 = lp_A1 + (hp_A1 - lp_A1) / 2;
  }
  
  if (volA1 - pp_A1 <= 0 && lastVolA1 - pp_A1 > 0.01)
  {
    rmsAC_A1 += peakA1 * peakA1;
    // full cycle of Sin Waves
    time = micros();
    peakA1 = 0;
    countHZ = countHZ + 1;
  }

  lastVolA1 = volA1;
  
  totalVoltageA1 += volA1;
  rmsA1 += volA1 * volA1;
  if (volA1 - pp_A1 > peakA1) peakA1 = volA1 - pp_A1;

  
  totalCnt++;

  // 816us
  clockPin = !clockPin;
  digitalWrite(7, clockPin);
  if (millis() - execMills >= 1000)
  {
      execMills = millis();

      dtostrf(sqrt(rmsAC_A1 / countHZ) * 0.707 / OP_A1, 1, 4, buffer);
//      tape = String(sqrt(rmsAC_A1 / countHZ) * 0.707 / OP_A1)+" "+String(sqrt(rmsA1 / totalCnt));
      tape = buffer;
      sprintf(buffer, "Samples: %d  HZ: %d   P-P: ", totalCnt, countHZ);
      Serial.print(buffer);
      Serial.print(hp_A1);
      Serial.print(" ");
      Serial.print(pp_A1);
      Serial.print(" ");
      Serial.print(lp_A1);
      Serial.print("   vol: ");
      Serial.print(a1);
      Serial.print("  DC RMS: ");
      Serial.print(sqrt(rmsA1 / totalCnt));
      Serial.print("  AC RMS: ");
      Serial.print(sqrt(rmsAC_A1 / countHZ) * 0.707);
      Serial.print("  AC Amps: ");
      Serial.println(sqrt(rmsAC_A1 / countHZ) * 0.707 / OP_A1);
      totalVoltageA1 = 0;
      rmsA1 = 0;
      peakA1 = 0;
      rmsAC_A1 = 0;
      totalCnt = 0;
      countHZ = 0;
      secondPass = secondPass + 1;
      if (secondPass >= 10)
      {
        secondPass = 0;
        hp_A1 = 0;
        lp_A1 = 5;
      }
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
  }
}
