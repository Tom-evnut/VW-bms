
#include <Arduino.h>
#include "config.h"
#include <ADC.h>

#include <FlexCAN.h>
#include <SPI.h>

#define CPU_REBOOT (_reboot_Teensyduino_());

EEPROMSettings settings;


//Simple BMS V2 wiring//
const int ACUR1 = A0; // current 1
const int ACUR2 = A1; // current 2
const int IN1 = 16; // input 1 - high active
const int IN2 = 17; // input 2- high active
const int IN3 = 19; // input 1 - high active
const int IN4 = 18; // input 2- high active
const int OUT1 = 11;// output 1 - high active
const int OUT2 = 12;// output 1 - high active
const int OUT3 = 20;// output 1 - high active
const int OUT4 = 21;// output 1 - high active
const int OUT5 = 22;// output 1 - high active
const int OUT6 = 23;// output 1 - high active
const int OUT7 = 5;// output 1 - high active
const int OUT8 = 6;// output 1 - high active
const int led = 13;
const int BMBfault = 11;

unsigned long looptime = 0;

//Debugging modes//////////////////
int debug = 1;
int inputcheck = 0; //read digital inputs
int outputcheck = 0; //check outputs
int candebug = 1; //view can frames
int debugCur = 0;
int menuload = 0;

char msgString[128];

ADC *adc = new ADC(); // adc object

CAN_message_t msg;
CAN_message_t inMsg;

uint32_t lastUpdate;

int controlid = 0x0BA;

void setup()
{
  delay(4000);  //just for easy debugging. It takes a few seconds for USB to come up properly on most OS's
  pinMode(ACUR1, INPUT);
  pinMode(ACUR2, INPUT);
  pinMode(IN1, INPUT);
  pinMode(IN2, INPUT);
  pinMode(IN3, INPUT);
  pinMode(IN4, INPUT);
  pinMode(OUT1, OUTPUT); // drive contactor
  pinMode(OUT2, OUTPUT); // precharge
  pinMode(OUT3, OUTPUT); // charge relay
  pinMode(OUT4, OUTPUT); // Negative contactor
  pinMode(OUT5, OUTPUT); // pwm driver output
  pinMode(OUT6, OUTPUT); // pwm driver output
  pinMode(OUT7, OUTPUT); // pwm driver output
  pinMode(OUT8, OUTPUT); // pwm driver output
  pinMode(led, OUTPUT);

  Can0.begin(500000);


  SERIALCONSOLE.begin(115200);
  SERIALCONSOLE.println("Starting up!");
}

void loop()
{
  while (Can0.available())
  {
    canread();
  }
    if (millis() - looptime > 500)
  {
    sendcommand();
    looptime = millis();
  }
}


void canread()
{
  Can0.read(inMsg);
  // Read data: len = data length, buf = data byte(s)

  if (candebug == 1)
  {
    Serial.print(millis());
    if ((inMsg.id & 0x80000000) == 0x80000000)    // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (inMsg.id & 0x1FFFFFFF), inMsg.len);
    else
      sprintf(msgString, ",0x%.3lX,false,%1d", inMsg.id, inMsg.len);

    Serial.print(msgString);

    if ((inMsg.id & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for (byte i = 0; i < inMsg.len; i++) {
        sprintf(msgString, ", 0x%.2X", inMsg.buf[i]);
        Serial.print(msgString);
      }
    }

    Serial.println();
  }
}

void sendcommand()
{
  msg.id  = controlid;
  msg.len = 8;
  msg.buf[0] = 0x00;
  msg.buf[1] = 0x00;
  msg.buf[2] = 0x00;
  msg.buf[3] = 0x00;
  msg.buf[4] = 0x00;
  msg.buf[5] = 0x00;
  msg.buf[6] = 0x00;
  msg.buf[7] = 0x00;
  Can0.write(msg);
  delay(2);
  msg.id  = controlid;
  msg.len = 8;
  msg.buf[0] = 0x45;
  msg.buf[1] = 0x01;
  msg.buf[2] = 0x28;
  msg.buf[3] = 0x00;
  msg.buf[4] = 0x00;
  msg.buf[5] = 0x00;
  msg.buf[6] = 0x00;
  msg.buf[7] = 0x30;
  Can0.write(msg);
}
