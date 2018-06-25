// CAN readout of VW type 5 modules
//

#include <mcp_can.h>
#include <SPI.h>

long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string
unsigned long looptime = 0;

#define CAN0_INT 2                              // Set INT to pin 2
MCP_CAN CAN0(9);                               // Set CS to pin 9 de[ending on shield used

int controlid = 0x0BA;
int moduleidstart = 0x1CC;
char mes[8] = {0, 0, 0, 0, 0, 0, 0, 0};


uint16_t voltage[30][14];
int modulespresent = 0;
int sent = 0;

int debug = 0;

void setup()
{
  Serial.begin(115200);

  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");

  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input

  Serial.println("Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
}

void loop()
{

  if (CAN0.checkReceive() == 3)                        // If CAN0_INT pin is low, read receive buffer
  {
    candecode();
  }
  if (Serial.available() > 0)
  {
    menu();
  }

  if (millis() > looptime + 500)
  {
    looptime = millis();
    for (int y = 0; y < modulespresent; y++)
    {
      Serial.println();
      Serial.print("Module ");
      Serial.print(y+1);
      Serial.print(" Voltages : ");
      for (int i = 1; i < 13; i++)
      {
        Serial.print(voltage[y][i]);
        Serial.print("mV ");
      }
    }

    Serial.println();
    sendcommand();
  }

}


void menu()
{
  byte incomingByte = Serial.read(); // read the incoming byte:

  switch (incomingByte)
  {
    case 's': //
      sendcommand();
      break;
  }
}

void sendcommand()
{
  mes[0] = 0x00;
  mes[1] = 0x00;
  mes[2] = 0x00;
  mes[3] = 0x00;
  mes[4] = 0x00;
  mes[5] = 0x00;
  mes[6] = 0x00;
  mes[7] = 0x00;
  CAN0.sendMsgBuf(controlid, 0, 8, mes);
  delay(50);
  mes[0] = 0x45;
  mes[1] = 0x01;
  mes[2] = 0x28;
  mes[3] = 0x00;
  mes[4] = 0x00;
  mes[5] = 0x00;
  mes[6] = 0x00;
  mes[7] = 0x30;
  CAN0.sendMsgBuf(controlid, 0, 8, mes);
  sent = 1;
  Serial.println();
  Serial.print("Command Sent");
  Serial.print(" Present Modules: ");
  Serial.print(modulespresent);
  Serial.println();
  modulespresent = 0;
}

void candecode()
{
  CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
  if (sent == 1)
  {
    moduleidstart = rxId;
    sent = 0;
  }
  if (rxId < 1024)
  {
    Serial.print("   ");
    int ID = rxId - moduleidstart;
    switch (ID)
    {
      case 0:
        voltage[modulespresent][1] = uint16_t(rxBuf[1] >> 4) + uint16_t(rxBuf[2] << 4) + 1000;
        voltage[modulespresent][3] = uint16_t(rxBuf[5] << 4) + uint16_t(rxBuf[4] >> 4) + 1000;

        voltage[modulespresent][2] = rxBuf[3] + uint16_t((rxBuf[4] & 0x0F) << 8) + 1000;
        voltage[modulespresent][4] = rxBuf[6] + uint16_t((rxBuf[7] & 0x0F) << 8) + 1000;
        break;
      case 1:
        voltage[modulespresent][5] = uint16_t(rxBuf[1] >> 4) + uint16_t(rxBuf[2] << 4) + 1000;
        voltage[modulespresent][7] = uint16_t(rxBuf[5] << 4) + uint16_t(rxBuf[4] >> 4) + 1000;

        voltage[modulespresent][6] = rxBuf[3] + uint16_t((rxBuf[4] & 0x0F) << 8) + 1000;
        voltage[modulespresent][8] = rxBuf[6] + uint16_t((rxBuf[7] & 0x0F) << 8) + 1000;
        break;

      case 2:
        voltage[modulespresent][9] = uint16_t(rxBuf[1] >> 4) + uint16_t(rxBuf[2] << 4) + 1000;
        voltage[modulespresent][11] = uint16_t(rxBuf[5] << 4) + uint16_t(rxBuf[4] >> 4) + 1000;

        voltage[modulespresent][10] = rxBuf[3] + uint16_t((rxBuf[4] & 0x0F) << 8) + 1000;
        voltage[modulespresent][12] = rxBuf[6] + uint16_t((rxBuf[7] & 0x0F) << 8) + 1000;
        modulespresent++;
        sent =1;
        break;
    }
  }
  if (debug == 1)                        // If CAN0_INT pin is low, read receive buffer
  {
    Serial.print(millis());
    if ((rxId & 0x80000000) == 0x80000000)    // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, ",0x%.3lX,false,%1d", rxId, len);

    Serial.print(msgString);

    if ((rxId & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
    } else {
      for (byte i = 0; i < len; i++) {
        sprintf(msgString, ", 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
      }
    }

    Serial.println();
  }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
