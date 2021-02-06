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

bool BMSModuleManager::checkcomms()
{
  int g = 0;
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      g = 1;
      if (modules[y].isReset())
      {
        //Do nothing as the counter has been reset
      }
      else
      {
        return false;
      }
    }
    modules[y].setReset(false);
    modules[y].setAddress(y);
  }
  if ( g == 0)
  {
    return false;
  }
  return true;
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

void BMSModuleManager::decodetemp(CAN_message_t &msg, int debug)
{
  int CMU = (msg.id & 0xFF);
  if (debug == 1)
  {
    Serial.println();
    Serial.print(CMU);
    Serial.print(" | ");
  }
  if (CMU > 10 && CMU < 60)
  {
    CMU = (CMU * 0.5) - 15;
  }
  /*
    if (CMU > 64 && CMU < 73)
    {
    CMU = CMU - 48;
    }
    if (CMU > 97 && CMU < 104)
    {
    CMU = CMU - 72;
    }
  */
  modules[CMU].decodetemp(msg);
  if (debug == 1 && CMU > 0 && CMU < 15)
  {
    Serial.println();
    Serial.print(CMU);
    Serial.print(" Temp Found");
  }
}

void BMSModuleManager::decodecan(CAN_message_t &msg, int debug)
{
  int CMU, Id = 0;
  switch (msg.id)
  {
    /*
      ///////////////// Two extender increment//////////
      case (0x210):
      CMU = 25;
      Id = 0;
      break;
      case (0x211):
      CMU = 25;
      Id = 1;
      break;
      case (0x212):
      CMU = 25;
      Id = 2;
      break;

      case (0x214):
      CMU = 26;
      Id = 0;
      break;
      case (0x215):
      CMU = 26;
      Id = 1;
      break;
      case (0x216):
      CMU = 26;
      Id = 2;
      break;
      case (0x218):
      CMU = 27;
      Id = 0;
      break;
      case (0x219):
      CMU = 27;
      Id = 1;
      break;
      case (0x21A):
      CMU = 27;
      Id = 2;
      break;
      case (0x21C):
      CMU = 28;
      Id = 0;
      break;
      case (0x21D):
      CMU = 28;
      Id = 1;
      break;
      case (0x21E):
      CMU = 28;
      Id = 2;
      break;

      case (0x220):
      CMU = 29;
      Id = 0;
      break;
      case (0x221):
      CMU = 29;
      Id = 1;
      break;
      case (0x222):
      CMU = 29;
      Id = 2;
      break;

      case (0x224):
      CMU = 30;
      Id = 0;
      break;
      case (0x225):
      CMU = 30;
      Id = 1;
      break;
      case (0x226):
      CMU = 30;
      Id = 2;
      break;

      case (0x228):
      CMU = 31;
      Id = 0;
      break;
      case (0x229):
      CMU = 31;
      Id = 1;
      break;
      case (0x22A):
      CMU = 31;
      Id = 2;
      break;

      case (0x22C):
      CMU = 32;
      Id = 0;
      break;
      case (0x22D):
      CMU = 32;
      Id = 1;
      break;
      case (0x22E):
      CMU = 32;
      Id = 2;
      break;


      ///////////////// Two extender increment//////////
      case (0x1F0):
      CMU = 17;
      Id = 0;
      break;
      case (0x1F1):
      CMU = 17;
      Id = 1;
      break;
      case (0x1F2):
      CMU = 17;
      Id = 2;
      break;

      case (0x1F4):
      CMU = 18;
      Id = 0;
      break;
      case (0x1F5):
      CMU = 18;
      Id = 1;
      break;
      case (0x1F6):
      CMU = 18;
      Id = 2;
      break;

      case (0x1F8):
      CMU = 19;
      Id = 0;
      break;
      case (0x1F9):
      CMU = 19;
      Id = 1;
      break;
      case (0x1FA):
      CMU = 19;
      Id = 2;
      break;

      case (0x1FC):
      CMU = 20;
      Id = 0;
      break;
      case (0x1FD):
      CMU = 20;
      Id = 1;
      break;
      case (0x1FE):
      CMU = 20;
      Id = 2;
      break;

      case (0x200):
      CMU = 21;
      Id = 0;
      break;
      case (0x201):
      CMU = 21;
      Id = 1;
      break;
      case (0x202):
      CMU = 21;
      Id = 2;
      break;

      case (0x204):
      CMU = 22;
      Id = 0;
      break;
      case (0x205):
      CMU = 22;
      Id = 1;
      break;
      case (0x206):
      CMU = 22;
      Id = 2;
      break;

      case (0x208):
      CMU = 23;
      Id = 0;
      break;
      case (0x209):
      CMU = 23;
      Id = 1;
      break;
      case (0x20A):
      CMU = 23;
      Id = 2;
      break;

      case (0x20C):
      CMU = 24;
      Id = 0;
      break;
      case (0x20D):
      CMU = 24;
      Id = 1;
      break;
      case (0x20E):
      CMU = 24;
      Id = 2;
      break;
    */
    ///////////////// one extender increment//////////

    case (0x1D0):
      CMU = 9;
      Id = 0;
      break;
    case (0x1D1):
      CMU = 9;
      Id = 1;
      break;
    case (0x1D2):
      CMU = 9;
      Id = 2;
      break;
    case (0x1D3):
      CMU = 9;
      Id = 3;
      break;

    case (0x1D4):
      CMU = 10;
      Id = 0;
      break;
    case (0x1D5):
      CMU = 10;
      Id = 1;
      break;
    case (0x1D6):
      CMU = 10;
      Id = 2;
      break;
    case (0x1D8):
      CMU = 11;
      Id = 0;
      break;
    case (0x1D9):
      CMU = 11;
      Id = 1;
      break;
    case (0x1DA):
      CMU = 11;
      Id = 2;
      break;
    case (0x1DC):
      CMU = 12;
      Id = 0;
      break;
    case (0x1DD):
      CMU = 12;
      Id = 1;
      break;
    case (0x1DE):
      CMU = 12;
      Id = 2;
      break;

    case (0x1E0):
      CMU = 13;
      Id = 0;
      break;
    case (0x1E1):
      CMU = 13;
      Id = 1;
      break;
    case (0x1E2):
      CMU = 13;
      Id = 2;
      break;

    case (0x1E4):
      CMU = 14;
      Id = 0;
      break;
    case (0x1E5):
      CMU = 14;
      Id = 1;
      break;
    case (0x1E6):
      CMU = 14;
      Id = 2;
      break;

    case (0x1E8):
      CMU = 15;
      Id = 0;
      break;
    case (0x1E9):
      CMU = 15;
      Id = 1;
      break;
    case (0x1EA):
      CMU = 15;
      Id = 2;
      break;

    case (0x1EC):
      CMU = 16;
      Id = 0;
      break;
    case (0x1ED):
      CMU = 16;
      Id = 1;
      break;
    case (0x1EE):
      CMU = 16;
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
    case (0x1B3):
      CMU = 1;
      Id = 3;
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
    case (0x1B7):
      CMU = 2;
      Id = 3;
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
    case (0x1BB):
      CMU = 3;
      Id = 3;
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
    case (0x1BF):
      CMU = 4;
      Id = 3;
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
    case (0x1C3):
      CMU = 5;
      Id = 3;
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
    case (0x1C7):
      CMU = 6;
      Id = 3;
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
    case (0x1CB):
      CMU = 7;
      Id = 3;
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
    case (0x1CF):
      CMU = 8;
      Id = 3;
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
    modules[CMU].setReset(true);
    modules[CMU].decodecan(Id, msg);
  }
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

void BMSModuleManager::setSensors(int sensor, float Ignore, float VoltDelta)
{
  for (int x = 1; x <= MAX_MODULE_ADDR; x++)
  {
    if (modules[x].isExisting())
    {
      modules[x].settempsensor(sensor);
      modules[x].setIgnoreCell(Ignore);
      modules[x].setDelta(VoltDelta);
    }
  }
}

float BMSModuleManager::getAvgTemperature()
{
  float avg = 0.0f;
  lowTemp = 999.0f;
  highTemp = -999.0f;
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
        if (modules[x].getHighTemp() > highTemp)
        {
          highTemp = modules[x].getHighTemp();
        }
        if (modules[x].getLowTemp() < lowTemp)
        {
          lowTemp = modules[x].getLowTemp();
        }
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

float BMSModuleManager::getHighTemperature()
{
  return highTemp;
}

float BMSModuleManager::getLowTemperature()
{
  return lowTemp;
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

void BMSModuleManager::printPackDetails(int digits)
{
  uint8_t faults;
  uint8_t alerts;
  uint8_t COV;
  uint8_t CUV;
  int cellNum = 0;

  Logger::console("");
  Logger::console("");
  Logger::console("");
  Logger::console("Modules: %i Cells: %i Strings: %i  Voltage: %fV   Avg Cell Voltage: %fV  Low Cell Voltage: %fV   High Cell Voltage: %fV Delta Voltage: %zmV   Avg Temp: %fC ", numFoundModules, seriescells(),
                  Pstring, getPackVoltage(), getAvgCellVolt(), LowCellVolt, HighCellVolt, (HighCellVolt - LowCellVolt) * 1000, getAvgTemperature());
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
      SERIALCONSOLE.print(modules[y].getModuleVoltage(), digits);
      SERIALCONSOLE.print("V");
      for (int i = 0; i < 13; i++)
      {
        if (cellNum < 10) SERIALCONSOLE.print(" ");
        SERIALCONSOLE.print("  Cell");
        SERIALCONSOLE.print(cellNum++);
        SERIALCONSOLE.print(": ");
        SERIALCONSOLE.print(modules[y].getCellVoltage(i), digits);
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
void BMSModuleManager::printAllCSV(unsigned long timestamp, float current, int SOC)
{
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      SERIALCONSOLE.print(timestamp);
      SERIALCONSOLE.print(",");
      SERIALCONSOLE.print(current, 0);
      SERIALCONSOLE.print(",");
      SERIALCONSOLE.print(SOC);
      SERIALCONSOLE.print(",");
      SERIALCONSOLE.print(y);
      SERIALCONSOLE.print(",");
      for (int i = 0; i < 8; i++)
      {
        SERIALCONSOLE.print(modules[y].getCellVoltage(i));
        SERIALCONSOLE.print(",");
      }
      SERIALCONSOLE.print(modules[y].getTemperature(0));
      SERIALCONSOLE.print(",");
      SERIALCONSOLE.print(modules[y].getTemperature(1));
      SERIALCONSOLE.print(",");
      SERIALCONSOLE.print(modules[y].getTemperature(2));
      SERIALCONSOLE.println();
    }
  }
  for (int y = 1; y < 63; y++)
  {
    if (modules[y].isExisting())
    {
      Serial2.print(timestamp);
      Serial2.print(",");
      Serial2.print(current, 0);
      Serial2.print(",");
      Serial2.print(SOC);
      Serial2.print(",");
      Serial2.print(y);
      Serial2.print(",");
      for (int i = 0; i < 13; i++)
      {
        Serial2.print(modules[y].getCellVoltage(i));
        Serial2.print(",");
      }
      Serial2.print(modules[y].getTemperature(0));
      Serial2.print(",");
      Serial2.print(modules[y].getTemperature(1));
      Serial2.print(",");
      Serial2.print(modules[y].getTemperature(2));
      Serial2.println();
    }
  }
}
