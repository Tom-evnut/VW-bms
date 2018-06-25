#include <FlexCAN.h>

unsigned long looptime = 0;
CAN_message_t msg;
CAN_message_t inMsg;
CAN_filter_t filter;

char msgString[128];
int controlid = 0x0BA;

void setup()
{
  delay(4000);  //just for easy debugging. It takes a few seconds for USB to come up properly on most OS's

  Can0.begin(500000);

 //set filters for standard
  for (int i = 0; i < 8; i++)
  {
    Can0.getFilter(filter, i);
    filter.flags.extended = 0;
    Can0.setFilter(filter, i);
  }
 //set filters for extended
  for (int i = 9; i < 13; i++)
  {
    Can0.getFilter(filter, i);
    filter.flags.extended = 1;
    Can0.setFilter(filter, i);
  }

  Serial.begin(115200);
  Serial.println("Starting up!");
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
