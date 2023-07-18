#include "config.h"
#include "BMSModule.h"
#include "Logger.h"


BMSModule::BMSModule()
{
  for (int i = 0; i < 13; i++)
  {
    cellVolt[i] = 0.0f;
    lowestCellVolt[i] = 5.0f;
    highestCellVolt[i] = 0.0f;
  }
  moduleVolt = 0.0f;
  temperatures[0] = 0.0f;
  temperatures[1] = 0.0f;
  temperatures[2] = 0.0f;
  lowestTemperature = 200.0f;
  highestTemperature = -100.0f;
  lowestModuleVolt = 200.0f;
  highestModuleVolt = 0.0f;
  balstat = 0;
  exists = false;
  reset = false;
  moduleAddress = 0;
  timeout = 30000; //milliseconds before comms timeout;
  type = 1;
}

void BMSModule::clearmodule()
{
  for (int i = 0; i < 13; i++)
  {
    cellVolt[i] = 0.0f;
  }
  moduleVolt = 0.0f;
  temperatures[0] = 0.0f;
  temperatures[1] = 0.0f;
  temperatures[2] = 0.0f;
  balstat = 0;
  exists = false;
  reset = false;
  moduleAddress = 0;
}

void BMSModule::decodetemp(CAN_message_t &msg, int y)
{
  if (y==1) //0x00 in byte 2 means its an MEB message
  {
    type = 1;
    if (msg.buf[7] == 0xFD)
    {
      if (msg.buf[2] != 0xFD)
      {
        temperatures[0] = (msg.buf[2] * 0.5) - 40;
      }
    }
    else
    {
      if (msg.buf[0] < 0xDF)
      {
        temperatures[0] = (msg.buf[0] * 0.5) - 43;
        balstat = msg.buf[2] + (msg.buf[3] << 8);
      }
      else
      {
        temperatures[0] = (msg.buf[3] * 0.5) - 43;
      }
      if (msg.buf[4] < 0xF0)
      {
        temperatures[1] = (msg.buf[4] * 0.5) - 43;
      }
      else
      {
        temperatures[1] = 0;
      }
      if (msg.buf[5] < 0xF0)
      {
        temperatures[2] = (msg.buf[5] * 0.5) - 43;
      }
      else
      {
        temperatures[2] = 0;
      }
    }
  }
  else
  {
    type = 2;
    temperatures[0] = ((uint16_t(((msg.buf[5] & 0x0F) << 4) | ((msg.buf[4] & 0xF0) >> 4))) * 0.5) - 40; //MEB Bits 36-44
  }
}

void BMSModule::decodecan(int Id, CAN_message_t &msg)
{
  switch (Id)
  {
    case 0:
      cmuerror = 0;
      cellVolt[0] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
      cellVolt[2] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
      cellVolt[1] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
      cellVolt[3] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;

      /*
        if (float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) < cellVolt[0] + VoltDelta && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > cellVolt[0] - VoltDelta || cellVolt[0] == 0)
        {
          cellVolt[0] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
        }
        else
        {
          cmuerror = 1;
        }
        if (float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) <  cellVolt[2] + VoltDelta && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > cellVolt[2] - VoltDelta || cellVolt[2] == 0)
        {
          cellVolt[2] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
        }
        else
        {
          cmuerror = 1;
        }
        if (float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[1] + VoltDelta && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[1] - VoltDelta || cellVolt[1] == 0)
        {
          cellVolt[1] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
          cmuerror = 1;
        }
        if (float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[3] + VoltDelta && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[3] - VoltDelta || cellVolt[3] == 0)
        {
          cellVolt[3] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
          cmuerror = 1;
        }
      */
      break;
    case 1:
      cmuerror = 0;
      cellVolt[4] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
      cellVolt[6] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
      cellVolt[5] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
      cellVolt[7] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;
      /*
        if (float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) < cellVolt[4] + VoltDelta && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > cellVolt[4] - VoltDelta || cellVolt[4] == 0)
        {
        cellVolt[4] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
        if (float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) <  cellVolt[6] + VoltDelta && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > cellVolt[6] - VoltDelta || cellVolt[6] == 0)
        {
        cellVolt[6] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
        if (float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[5] + VoltDelta && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[5] - VoltDelta || cellVolt[5] == 0)
        {
        cellVolt[5] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
        if (float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[7] + VoltDelta && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[7] - VoltDelta || cellVolt[7] == 0)
        {
        cellVolt[7] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
      */
      break;

    case 2:
      cmuerror = 0;
      cellVolt[8] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
      cellVolt[10] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
      cellVolt[9] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
      cellVolt[11] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;
      /*
        if (float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) < cellVolt[8] + VoltDelta && float((uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001) > cellVolt[8] - VoltDelta || cellVolt[8] == 0)
        {
        cellVolt[8] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }

        if (float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > 0 && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) <  cellVolt[10] + VoltDelta && float((uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001) > cellVolt[10] - VoltDelta || cellVolt[10] == 0)
        {
        cellVolt[10] = (uint16_t(msg.buf[5] << 4) + uint16_t(msg.buf[4] >> 4) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }

        if (float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[9] + VoltDelta && float((msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[9] - VoltDelta || cellVolt[9] == 0)
        {
        cellVolt[9] = (msg.buf[3] + uint16_t((msg.buf[4] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
        if (float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > 0 && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) < cellVolt[11] + VoltDelta && float((msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001) > cellVolt[11] - VoltDelta || cellVolt[11] == 0)
        {
        cellVolt[11] = (msg.buf[6] + uint16_t((msg.buf[7] & 0x0F) << 8) + 1000) * 0.001;
        }
        else
        {
        cmuerror = 1;
        }
      */
      break;

    case 3:
      cmuerror = 0;
      cellVolt[12] = (uint16_t(msg.buf[1] >> 4) + uint16_t(msg.buf[2] << 4) + 1000) * 0.001;
      break;

    default:
      break;

  }
  if (getLowTemp() < lowestTemperature) lowestTemperature = getLowTemp();
  if (getHighTemp() > highestTemperature) highestTemperature = getHighTemp();

  for (int i = 0; i < 13; i++)
  {
    if (lowestCellVolt[i] > cellVolt[i] && cellVolt[i] >= IgnoreCell) lowestCellVolt[i] = cellVolt[i];
    if (highestCellVolt[i] < cellVolt[i] && cellVolt[i] > 5.0) highestCellVolt[i] = cellVolt[i];
  }

  if (cmuerror == 0)
  {
    lasterror = millis();
  }
  else
  {
    if (millis() - lasterror < timeout)
    {
      if (lasterror + timeout - millis() < 5000)
      {
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("Module");
        SERIALCONSOLE.print(moduleAddress);
        SERIALCONSOLE.print("Counter Till Can Error : ");
        SERIALCONSOLE.println(lasterror + timeout - millis() );
      }
    }
    else
    {
      for (int i = 0; i < 8; i++)
      {
        cellVolt[i] = 0.0f;
      }
      moduleVolt = 0.0f;
      temperatures[0] = 0.0f;
      temperatures[1] = 0.0f;
      temperatures[2] = 0.0f;
    }
  }
}


/*
  Reading the status of the board to identify any flags, will be more useful when implementing a sleep cycle
*/

uint8_t BMSModule::getFaults()
{
  return faults;
}

uint8_t BMSModule::getAlerts()
{
  return alerts;
}

uint8_t BMSModule::getCOVCells()
{
  return COVFaults;
}

uint8_t BMSModule::getCUVCells()
{
  return CUVFaults;
}

float BMSModule::getCellVoltage(int cell)
{
  if (cell < 0 || cell > 13) return 0.0f;
  return cellVolt[cell];
}

float BMSModule::getLowCellV()
{
  float lowVal = 10.0f;
  for (int i = 0; i < 13; i++) if (cellVolt[i] < lowVal && cellVolt[i] > IgnoreCell) lowVal = cellVolt[i];
  return lowVal;
}

float BMSModule::getHighCellV()
{
  float hiVal = 0.0f;
  for (int i = 0; i < 13; i++)
    if (cellVolt[i] > IgnoreCell && cellVolt[i] < 5.0)
    {
      if (cellVolt[i] > hiVal) hiVal = cellVolt[i];
    }
  return hiVal;
}

float BMSModule::getAverageV()
{
  int x = 0;
  float avgVal = 0.0f;
  for (int i = 0; i < 13; i++)
  {
    if (cellVolt[i] > IgnoreCell && cellVolt[i] < 5.0)
    {
      x++;
      avgVal += cellVolt[i];
    }
  }

  scells = x;
  avgVal /= x;

  if (scells == 0)
  {
    avgVal = 0;
  }

  return avgVal;
}

int BMSModule::getscells()
{
  return scells;
}

float BMSModule::getHighestModuleVolt()
{
  return highestModuleVolt;
}

float BMSModule::getLowestModuleVolt()
{
  return lowestModuleVolt;
}

float BMSModule::getHighestCellVolt(int cell)
{
  if (cell < 0 || cell > 13) return 0.0f;
  return highestCellVolt[cell];
}

float BMSModule::getLowestCellVolt(int cell)
{
  if (cell < 0 || cell > 13) return 0.0f;
  return lowestCellVolt[cell];
}

float BMSModule::getHighestTemp()
{
  return highestTemperature;
}

float BMSModule::getLowestTemp()
{
  return lowestTemperature;
}

float BMSModule::getLowTemp()
{
  if (type == 1)
  {
    if (sensor == 0)
    {
      if (getAvgTemp() > 0.5)
      {
        if (temperatures[0] > 0.5)
        {
          if (temperatures[0] < temperatures[1] && temperatures[0] < temperatures[2])
          {
            return (temperatures[0]);
          }
        }
        if (temperatures[1] > 0.5)
        {
          if (temperatures[1] < temperatures[0] && temperatures[1] < temperatures[2])
          {
            return (temperatures[1]);
          }
        }
        if (temperatures[2] > 0.5)
        {
          if (temperatures[2] < temperatures[1] && temperatures[2] < temperatures[0])
          {
            return (temperatures[2]);
          }
        }
      }
    }
    else
    {
      return temperatures[sensor - 1];
    }
  }
  else
  {
    return temperatures[0];
  }
  return 0.0f;
}

float BMSModule::getHighTemp()
{
  if (type == 1)
  {
    if (sensor == 0)
    {
      return (temperatures[0] < temperatures[1]) ? temperatures[1] : temperatures[0];
    }
    else
    {
      return temperatures[sensor - 1];
    }
  }
  else
  {
    return temperatures[0];
  }
}

float BMSModule::getAvgTemp()
{
  if (type == 1)
  {
    if (sensor == 0)
    {
      if ((temperatures[0] + temperatures[1] + temperatures[2]) / 3.0f > 0.5)
      {
        if (temperatures[0] > 0.5 && temperatures[1] > 0.5 && temperatures[2] > 0.5)
        {
          return (temperatures[0] + temperatures[1] + temperatures[2]) / 3.0f;
        }
        if (temperatures[0] < 0.5 && temperatures[1] > 0.5 && temperatures[2] > 0.5)
        {
          return (temperatures[1] + temperatures[2]) / 2.0f;
        }
        if (temperatures[0] > 0.5 && temperatures[1] < 0.5 && temperatures[2] > 0.5)
        {
          return (temperatures[0] + temperatures[2]) / 2.0f;
        }
        if (temperatures[0] > 0.5 && temperatures[1] > 0.5 && temperatures[2] < 0.5)
        {
          return (temperatures[0] + temperatures[1]) / 2.0f;
        }
        if (temperatures[0] > 0.5 && temperatures[1] < 0.5 && temperatures[2] < 0.5)
        {
          return (temperatures[0]);
        }
        if (temperatures[0] < 0.5 && temperatures[1] > 0.5 && temperatures[2] < 0.5)
        {
          return (temperatures[1]);
        }
        if (temperatures[0] < 0.5 && temperatures[1] < 0.5 && temperatures[2] > 0.5)
        {
          return (temperatures[2]);
        }
        if (temperatures[0] < 0.5 && temperatures[1] < 0.5 && temperatures[2] < 0.5)
        {
          return (-80);
        }
      }
    }
    else
    {
      return temperatures[sensor - 1];
    }
  }
  else
  {
    return temperatures[0];
  }
  return 0.0f;
}

float BMSModule::getModuleVoltage()
{
  moduleVolt = 0;
  for (int i = 0; i < 13; i++)
  {
    if (cellVolt[i] > IgnoreCell && cellVolt[i] < 5.0)
    {
      moduleVolt = moduleVolt + cellVolt[i];
    }
  }
  return moduleVolt;
}

float BMSModule::getTemperature(int temp)
{
  if (temp < 0 || temp > 2) return 0.0f;
  return temperatures[temp];
}

void BMSModule::setAddress(int newAddr)
{
  if (newAddr < 0 || newAddr > MAX_MODULE_ADDR) return;
  moduleAddress = newAddr;
}

int BMSModule::getAddress()
{
  return moduleAddress;
}

int BMSModule::getType()
{
  return type;
}

int BMSModule::getBalStat()
{
  return balstat;
}

bool BMSModule::isExisting()
{
  return exists;
}

bool BMSModule::isReset()
{
  return reset;
}

void BMSModule::settempsensor(int tempsensor)
{
  sensor = tempsensor;
}

void BMSModule::setExists(bool ex)
{
  exists = ex;
}

void BMSModule::setDelta(float ex)
{
  VoltDelta = ex;
}

void BMSModule::setReset(bool ex)
{
  reset = ex;
}

void BMSModule::setIgnoreCell(float Ignore)
{
  IgnoreCell = Ignore;
  Serial.println();
  Serial.println();
  Serial.println(Ignore);
  Serial.println();

}
