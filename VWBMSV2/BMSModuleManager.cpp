#include "config.h"
#include "BMSModuleManager.h"
#include "BMSUtil.h"
#include "Logger.h"

extern EEPROMSettings settings;

BMSModuleManager::BMSModuleManager()
{
  for (int i = 1; i <= MAX_MODULE_ADDR; i++) {
    modules[i].setExists(false);
    modules[i].setAddress(i);
  }
  lowestPackVolt = 1000.0f;
  highestPackVolt = 0.0f;
  lowestPackTemp = 200.0f;
  highestPackTemp = -100.0f;
  isFaulted = false;
}

int BMSModuleManager::seriescells()
{
  spack = 0;
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      spack = spack + modules[y].getscells();
    }
  }
  return spack;
}

void BMSModuleManager::clearmodules()
{
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      modules[y].clearmodule();
      modules[y].setExists(false);
      modules[y].setAddress(y);
    }
  }
}

void BMSModuleManager::decodecan(CAN_message_t &msg, int debug)
{
  int CMU, Id = 0;
  switch (msg.id)
  {
    ///////////////// Two extender increment//////////
    case (0x100):
      CMU = 31;
      Id = 0;
      break;
    case (0x101):
      CMU = 31;
      Id = 1;
      break;
    case (0x102):
      CMU = 31;
      Id = 2;
      break;

    case (0x104):
      CMU = 32;
      Id = 0;
      break;
    case (0x105):
      CMU = 32;
      Id = 1;
      break;
    case (0x106):
      CMU = 32;
      Id = 2;
      break;
    case (0x108):
      CMU = 33;
      Id = 0;
      break;
    case (0x109):
      CMU = 33;
      Id = 1;
      break;
    case (0x10A):
      CMU = 33;
      Id = 2;
      break;
    case (0x10C):
      CMU = 34;
      Id = 0;
      break;
    case (0x10D):
      CMU = 34;
      Id = 1;
      break;
    case (0x10E):
      CMU = 34;
      Id = 2;
      break;

    case (0x110):
      CMU = 35;
      Id = 0;
      break;
    case (0x111):
      CMU = 35;
      Id = 1;
      break;
    case (0x112):
      CMU = 35;
      Id = 2;
      break;

    case (0x114):
      CMU = 36;
      Id = 0;
      break;
    case (0x115):
      CMU = 36;
      Id = 1;
      break;
    case (0x116):
      CMU = 36;
      Id = 2;
      break;

    case (0x118):
      CMU = 37;
      Id = 0;
      break;
    case (0x119):
      CMU = 37;
      Id = 1;
      break;
    case (0x11A):
      CMU = 37;
      Id = 2;
      break;

    case (0x11C):
      CMU = 38;
      Id = 0;
      break;
    case (0x11D):
      CMU = 38;
      Id = 1;
      break;
    case (0x11E):
      CMU = 38;
      Id = 2;
      break;


    ///////////////// Two extender increment//////////
    case (0x130):
      CMU = 21;
      Id = 0;
      break;
    case (0x131):
      CMU = 21;
      Id = 1;
      break;
    case (0x132):
      CMU = 21;
      Id = 2;
      break;

    case (0x134):
      CMU = 22;
      Id = 0;
      break;
    case (0x135):
      CMU = 22;
      Id = 1;
      break;
    case (0x136):
      CMU = 22;
      Id = 2;
      break;
    case (0x138):
      CMU = 23;
      Id = 0;
      break;
    case (0x139):
      CMU = 23;
      Id = 1;
      break;
    case (0x13A):
      CMU = 23;
      Id = 2;
      break;
    case (0x13C):
      CMU = 24;
      Id = 0;
      break;
    case (0x13D):
      CMU = 24;
      Id = 1;
      break;
    case (0x13E):
      CMU = 24;
      Id = 2;
      break;

    case (0x140):
      CMU = 25;
      Id = 0;
      break;
    case (0x141):
      CMU = 25;
      Id = 1;
      break;
    case (0x142):
      CMU = 25;
      Id = 2;
      break;

    case (0x144):
      CMU = 26;
      Id = 0;
      break;
    case (0x145):
      CMU = 26;
      Id = 1;
      break;
    case (0x146):
      CMU = 26;
      Id = 2;
      break;

    case (0x148):
      CMU = 27;
      Id = 0;
      break;
    case (0x149):
      CMU = 27;
      Id = 1;
      break;
    case (0x14A):
      CMU = 27;
      Id = 2;
      break;

    case (0x14C):
      CMU = 28;
      Id = 0;
      break;
    case (0x14D):
      CMU = 28;
      Id = 1;
      break;
    case (0x14E):
      CMU = 28;
      Id = 2;
      break;

    ///////////////// one extender increment//////////
    case (0x170):
      CMU = 11;
      Id = 0;
      break;
    case (0x171):
      CMU = 11;
      Id = 1;
      break;
    case (0x172):
      CMU = 11;
      Id = 2;
      break;

    case (0x174):
      CMU = 12;
      Id = 0;
      break;
    case (0x175):
      CMU = 12;
      Id = 1;
      break;
    case (0x176):
      CMU = 12;
      Id = 2;
      break;
    case (0x178):
      CMU = 13;
      Id = 0;
      break;
    case (0x179):
      CMU = 13;
      Id = 1;
      break;
    case (0x17A):
      CMU = 13;
      Id = 2;
      break;
    case (0x17C):
      CMU = 14;
      Id = 0;
      break;
    case (0x17D):
      CMU = 14;
      Id = 1;
      break;
    case (0x17E):
      CMU = 14;
      Id = 2;
      break;

    case (0x180):
      CMU = 15;
      Id = 0;
      break;
    case (0x181):
      CMU = 15;
      Id = 1;
      break;
    case (0x182):
      CMU = 15;
      Id = 2;
      break;

    case (0x184):
      CMU = 16;
      Id = 0;
      break;
    case (0x185):
      CMU = 16;
      Id = 1;
      break;
    case (0x186):
      CMU = 16;
      Id = 2;
      break;

    case (0x188):
      CMU = 17;
      Id = 0;
      break;
    case (0x189):
      CMU = 17;
      Id = 1;
      break;
    case (0x18A):
      CMU = 17;
      Id = 2;
      break;

    case (0x18C):
      CMU = 18;
      Id = 0;
      break;
    case (0x18D):
      CMU = 18;
      Id = 1;
      break;
    case (0x18E):
      CMU = 18;
      Id = 2;
      break;


    ///////////////////////standard ids////////////////

    case (0x1B0):
      CMU = 1;
      Id = 0;
      break;
    case (0x1B1):
      CMU = 1;
      Id = 1;
      break;
    case (0x1B2):
      CMU = 1;
      Id = 2;
      break;

    case (0x1B4):
      CMU = 2;
      Id = 0;
      break;
    case (0x1B5):
      CMU = 2;
      Id = 1;
      break;
    case (0x1B6):
      CMU = 2;
      Id = 2;
      break;

    case (0x1B8):
      CMU = 3;
      Id = 0;
      break;
    case (0x1B9):
      CMU = 3;
      Id = 1;
      break;
    case (0x1BA):
      CMU = 3;
      Id = 2;
      break;

    case (0x1BC):
      CMU = 4;
      Id = 0;
      break;
    case (0x1BD):
      CMU = 4;
      Id = 1;
      break;
    case (0x1BE):
      CMU = 4;
      Id = 2;
      break;

    case (0x1C0):
      CMU = 5;
      Id = 0;
      break;
    case (0x1C1):
      CMU = 5;
      Id = 1;
      break;
    case (0x1C2):
      CMU = 5;
      Id = 2;
      break;

    case (0x1C4):
      CMU = 6;
      Id = 0;
      break;
    case (0x1C5):
      CMU = 6;
      Id = 1;
      break;
    case (0x1C6):
      CMU = 6;
      Id = 2;
      break;

    case (0x1C8):
      CMU = 7;
      Id = 0;
      break;
    case (0x1C9):
      CMU = 7;
      Id = 1;
      break;
    case (0x1CA):
      CMU = 7;
      Id = 2;
      break;

    case (0x1CC):
      CMU = 8;
      Id = 0;
      break;
    case (0x1CD):
      CMU = 8;
      Id = 1;
      break;
    case (0x1CE):
      CMU = 8;
      Id = 2;
      break;

    default:
      return;
      break;
  }
  if (CMU > 0 && CMU < 64)
  {
    if (debug == 1)
    {
      Serial.println();
      Serial.print(CMU);
      Serial.print(",");
      Serial.print(Id);
      Serial.println();
    }
    modules[CMU].setExists(true);
    modules[CMU].decodecan(Id, msg);
  }
}

void BMSModuleManager::balanceCells()
{
  /*
    uint8_t payload[4];
    uint8_t buff[30];
    uint8_t balance = 0;//bit 0 - 5 are to activate cell balancing 1-6

    for (int address = 1; address <= MAX_MODULE_ADDR; address++)
    {
        balance = 0;
        for (int i = 0; i < 6; i++)
        {
            if (getLowCellVolt() < modules[address].getCellVoltage(i))
            {
                balance = balance | (1<<i);
            }
        }

        if (balance != 0) //only send balance command when needed
        {
            payload[0] = address << 1;
            payload[1] = REG_BAL_TIME;
            payload[2] = 0x05; //5 second balance limit, if not triggered to balance it will stop after 5 seconds
            BMSUtil::sendData(payload, 3, true);
            delay(2);
            BMSUtil::getReply(buff, 30);

            payload[0] = address << 1;
            payload[1] = REG_BAL_CTRL;
            payload[2] = balance; //write balance state to register
            BMSUtil::sendData(payload, 3, true);
            delay(2);
            BMSUtil::getReply(buff, 30);

            if (Logger::isDebug()) //read registers back out to check if everthing is good
            {
                delay(50);
                payload[0] = address << 1;
                payload[1] = REG_BAL_TIME;
                payload[2] = 1; //
                BMSUtil::sendData(payload, 3, false);
                delay(2);
                BMSUtil::getReply(buff, 30);

                payload[0] = address << 1;
                payload[1] = REG_BAL_CTRL;
                payload[2] = 1; //
                BMSUtil::sendData(payload, 3, false);
                delay(2);
                BMSUtil::getReply(buff, 30);
            }
        }
    }
  */
}

/*
   Try to set up any unitialized boards. Send a command to address 0 and see if there is a response. If there is then there is
   still at least one unitialized board. Go ahead and give it the first ID not registered as already taken.
   If we send a command to address 0 and no one responds then every board is inialized and this routine stops.
   Don't run this routine until after the boards have already been enumerated.\
   Note: The 0x80 conversion it is looking might in theory block the message from being forwarded so it might be required
   To do all of this differently. Try with multiple boards. The alternative method would be to try to set the next unused
   address and see if any boards respond back saying that they set the address.
*/
void BMSModuleManager::setupBoards()
{

}
/*
   Iterate through all 62 possible board addresses (1-62) to see if they respond
*/
void BMSModuleManager::findBoards()
{
}


/*
   Force all modules to reset back to address 0 then set them all up in order so that the first module
   in line from the master board is 1, the second one 2, and so on.
*/
void BMSModuleManager::renumberBoardIDs()
{

}

/*
  After a RESET boards have their faults written due to the hard restart or first time power up, this clears thier faults
*/
void BMSModuleManager::clearFaults()
{
}

/*
  Puts all boards on the bus into a Sleep state, very good to use when the vehicle is a rest state.
  Pulling the boards out of sleep only to check voltage decay and temperature when the contactors are open.
*/

void BMSModuleManager::sleepBoards()
{
}

/*
  Wakes all the boards up and clears thier SLEEP state bit in the Alert Status Registery
*/

void BMSModuleManager::wakeBoards()
{
}

void BMSModuleManager::getAllVoltTemp()
{
  packVolt = 0.0f;
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      Logger::debug("");
      Logger::debug("Module %i exists. Reading voltage and temperature values", x);
      modules[x].readModuleValues();
      Logger::debug("Module voltage: %f", modules[x].getModuleVoltage());
      Logger::debug("Lowest Cell V: %f     Highest Cell V: %f", modules[x].getLowCellV(), modules[x].getHighCellV());
      Logger::debug("Temp1: %f       Temp2: %f", modules[x].getTemperature(0), modules[x].getTemperature(1));
      packVolt += modules[x].getModuleVoltage();
      if (modules[x].getLowTemp() < lowestPackTemp) lowestPackTemp = modules[x].getLowTemp();
      if (modules[x].getHighTemp() > highestPackTemp) highestPackTemp = modules[x].getHighTemp();
    }
  }

  packVolt = packVolt / Pstring;
  if (packVolt > highestPackVolt) highestPackVolt = packVolt;
  if (packVolt < lowestPackVolt) lowestPackVolt = packVolt;

  if (digitalRead(11) == LOW) {
    if (!isFaulted) Logger::error("One or more BMS modules have entered the fault state!");
    isFaulted = true;
  }
  else
  {
    if (isFaulted) Logger::info("All modules have exited a faulted state");
    isFaulted = false;
  }
}

float BMSModuleManager::getLowCellVolt()
{
  LowCellVolt = 5.0;
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      if (modules[x].getLowCellV() <  LowCellVolt)  LowCellVolt = modules[x].getLowCellV();
    }
  }
  return LowCellVolt;
}

float BMSModuleManager::getHighCellVolt()
{
  HighCellVolt = 0.0;
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      if (modules[x].getHighCellV() >  HighCellVolt)  HighCellVolt = modules[x].getHighCellV();
    }
  }
  return HighCellVolt;
}

float BMSModuleManager::getPackVoltage()
{
  return packVolt;
}

float BMSModuleManager::getLowVoltage()
{
  return lowestPackVolt;
}

float BMSModuleManager::getHighVoltage()
{
  return highestPackVolt;
}

void BMSModuleManager::setBatteryID(int id)
{
  batteryID = id;
}

void BMSModuleManager::setPstrings(int Pstrings)
{
  Pstring = Pstrings;
}

void BMSModuleManager::setSensors(int sensor, float Ignore)
{
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      modules[x].settempsensor(sensor);
      modules[x].setIgnoreCell(Ignore);
    }
  }
}

float BMSModuleManager::getAvgTemperature()
{
  float avg = 0.0f;
  int y = 0; //counter for modules below -70 (no sensors connected)
  numFoundModules = 0;
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      numFoundModules++;
      if (modules[x].getAvgTemp() > -70)
      {
        avg += modules[x].getAvgTemp();
      }
      else
      {
        y++;
      }
    }
  }
  avg = avg / (float)(numFoundModules - y);

  return avg;
}

float BMSModuleManager::getAvgCellVolt()
{
  float avg = 0.0f;
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting()) avg += modules[x].getAverageV();
  }
  avg = avg / (float)numFoundModules;

  return avg;
}

void BMSModuleManager::printPackSummary()
{
  uint8_t faults;
  uint8_t alerts;
  uint8_t COV;
  uint8_t CUV;

  Logger::console("");
  Logger::console("");
  Logger::console("");
  Logger::console("Modules: %i  Cells: %i  Voltage: %fV   Avg Cell Voltage: %fV     Avg Temp: %fC ", numFoundModules, seriescells(),
                  getPackVoltage(), getAvgCellVolt(), getAvgTemperature());
  Logger::console("");
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      faults = modules[y].getFaults();
      alerts = modules[y].getAlerts();
      COV = modules[y].getCOVCells();
      CUV = modules[y].getCUVCells();

      Logger::console("                               Module #%i", y);

      Logger::console("  Voltage: %fV   (%fV-%fV)     Temperatures: (%fC-%fC)", modules[y].getModuleVoltage(),
                      modules[y].getLowCellV(), modules[y].getHighCellV(), modules[y].getLowTemp(), modules[y].getHighTemp());
      if (faults > 0)
      {
        Logger::console("  MODULE IS FAULTED:");
        if (faults & 1)
        {
          SERIALCONSOLE.print("    Overvoltage Cell Numbers (1-6): ");
          for (int i = 0; i < 12; i++)
          {
            if (COV & (1 << i))
            {
              SERIALCONSOLE.print(i + 1);
              SERIALCONSOLE.print(" ");
            }
          }
          SERIALCONSOLE.println();
        }
        if (faults & 2)
        {
          SERIALCONSOLE.print("    Undervoltage Cell Numbers (1-6): ");
          for (int i = 0; i < 12; i++)
          {
            if (CUV & (1 << i))
            {
              SERIALCONSOLE.print(i + 1);
              SERIALCONSOLE.print(" ");
            }
          }
          SERIALCONSOLE.println();
        }
        if (faults & 4)
        {
          Logger::console("    CRC error in received packet");
        }
        if (faults & 8)
        {
          Logger::console("    Power on reset has occurred");
        }
        if (faults & 0x10)
        {
          Logger::console("    Test fault active");
        }
        if (faults & 0x20)
        {
          Logger::console("    Internal registers inconsistent");
        }
      }
      if (alerts > 0)
      {
        Logger::console("  MODULE HAS ALERTS:");
        if (alerts & 1)
        {
          Logger::console("    Over temperature on TS1");
        }
        if (alerts & 2)
        {
          Logger::console("    Over temperature on TS2");
        }
        if (alerts & 4)
        {
          Logger::console("    Sleep mode active");
        }
        if (alerts & 8)
        {
          Logger::console("    Thermal shutdown active");
        }
        if (alerts & 0x10)
        {
          Logger::console("    Test Alert");
        }
        if (alerts & 0x20)
        {
          Logger::console("    OTP EPROM Uncorrectable Error");
        }
        if (alerts & 0x40)
        {
          Logger::console("    GROUP3 Regs Invalid");
        }
        if (alerts & 0x80)
        {
          Logger::console("    Address not registered");
        }
      }
      if (faults > 0 || alerts > 0) SERIALCONSOLE.println();
    }
  }
}

void BMSModuleManager::printPackDetails()
{
  uint8_t faults;
  uint8_t alerts;
  uint8_t COV;
  uint8_t CUV;
  int cellNum = 0;

  Logger::console("");
  Logger::console("");
  Logger::console("");
  Logger::console("Modules: %i Cells: %i  Voltage: %fV   Avg Cell Voltage: %fV  Low Cell Voltage: %fV   High Cell Voltage: %fV   Avg Temp: %fC ", numFoundModules, seriescells(),
                  getPackVoltage(), getAvgCellVolt(), LowCellVolt, HighCellVolt, getAvgTemperature());
  Logger::console("");
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      faults = modules[y].getFaults();
      alerts = modules[y].getAlerts();
      COV = modules[y].getCOVCells();
      CUV = modules[y].getCUVCells();

      SERIALCONSOLE.print("Module #");
      SERIALCONSOLE.print(y);
      if (y < 10) SERIALCONSOLE.print(" ");
      SERIALCONSOLE.print("  ");
      SERIALCONSOLE.print(modules[y].getModuleVoltage());
      SERIALCONSOLE.print("V");
      for (int i = 0; i < 12; i++)
      {
        if (cellNum < 10) SERIALCONSOLE.print(" ");
        SERIALCONSOLE.print("  Cell");
        SERIALCONSOLE.print(cellNum++);
        SERIALCONSOLE.print(": ");
        SERIALCONSOLE.print(modules[y].getCellVoltage(i));
        SERIALCONSOLE.print("V");
      }
      SERIALCONSOLE.println();
      SERIALCONSOLE.print(" Temp 1: ");
      SERIALCONSOLE.print(modules[y].getTemperature(0));
      SERIALCONSOLE.print("C Temp 2: ");
      SERIALCONSOLE.print(modules[y].getTemperature(1));
      SERIALCONSOLE.print("C Temp 3: ");
      SERIALCONSOLE.print(modules[y].getTemperature(2));
      SERIALCONSOLE.println("C");

    }
  }
}
