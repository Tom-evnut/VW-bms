/*
  Copyright (c) 2019 Simp ECO Engineering
  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/********************************
 Port notes:
Clone those libraries:
https://github.com/JonHub/Filters
https://github.com/tonton81/FlexCAN_T4
https://github.com/tonton81/WDT_T4
in ~/Arduino/libraries
 ********************************/
#include "config.h"
#include "BMSModuleManager.h"
#include <Arduino.h>
#include "SerialConsole.h"
#include "Logger.h"
#include <ADC.h>  //https://github.com/pedvide/ADC
#include <EEPROM.h>

#include <SPI.h>
#include <Filters.h>  //https://github.com/JonHub/Filters
#include "BMSUtil.h"

#define CPU_REBOOT (_reboot_Teensyduino_());

#ifdef USING_TEENSY4
#include <FlexCAN_T4.h>  //https://github.com/tonton81/FlexCAN_T4
#include <circular_buffer.h>
#include <imxrt_flexcan.h>
#include <isotp.h>
#include <isotp_server.h>
#include <kinetis_flexcan.h>
#include "Watchdog_t4.h" //https://github.com/tonton81/WDT_T4
WDT_T4<WDT1> wdt;
/********************************
 Port notes:
 WDOG_TOVALL The bare metal watchdog register acces must be replaced
 PMC_LVDSC2  The bare metal low voltage irq must be replaced. 
 */
#else
#include <FlexCAN.h>  //https://github.com/collin80/FlexCAN_Library
#endif                //USING_TEENSY4

BMSModuleManager bms;
SerialConsole console;
EEPROMSettings settings;

/////Version Identifier/////////
int firmver = 230920;

//Curent filter//
float filterFrequency = 5.0;
FilterOnePole lowpassFilter(LOWPASS, filterFrequency);


//Simple BMS V2 wiring//
#ifdef USING_TEENSY4
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> Can0;

// A0 and A1 are needed for serial3 used on the Tesla BMS
// Take them on IN1 and IN2
// Some pins must be cut on the adapter
const int ACUR2 = A2;  // current 1
const int ACUR1 = A3;  // current 2
const int IN1 = 9;     // input 1 - high active
const int IN2 = 10;    // input 2- high active
#else
const int ACUR2 = A0;  // current 1o
const int ACUR1 = A1;  // current 2
const int IN1 = 17;    // input 1 - high active
const int IN2 = 16;    // input 2- high active
#endif                //USING_TEENSY4
const int IN3 = 18;   // input 1 - high active
const int IN4 = 19;   // input 2- high active
const int OUT1 = 11;  // output 1 - high active
const int OUT2 = 12;  // output 1 - high active
const int OUT3 = 20;  // output 1 - high active
const int OUT4 = 21;  // output 1 - high active
#ifdef USING_TEENSY4
// Swaped with Teensy 4.0 CAN1
const int OUT5 = 3;  // output 1 - high active
const int OUT6 = 4;  // output 1 - high active
#else
const int OUT5 = 22;   // output 1 - high active
const int OUT6 = 23;   // output 1 - high active
#endif               //USING_TEENSY4
const int OUT7 = 5;  // output 1 - high active
const int OUT8 = 6;  // output 1 - high active
const int led = 13;
const int BMBfault = 11;

byte bmsstatus = 0;
//bms status values
#define Boot 0
#define Ready 1
#define Drive 2
#define Charge 3
#define Precharge 4
#define Error 5
//

//Current sensor values
#define Undefined 0
#define Analoguedual 1
#define Canbus 2
#define Analoguesing 3
//

// Can current sensor values
#define LemCAB300 1
#define IsaScale 3
#define VictronLynx 4
#define LemCAB500 2
#define CurCanMax 4  // max value

//Charger Types
#define NoCharger 0
#define BrusaNLG5 1
#define ChevyVolt 2
#define Eltek 3
#define Elcon 4
#define Victron 5
#define Coda 6
#define Pylon 7  //testing only
#define Outlander 8

//

int Discharge;
int ErrorReason = 0;

//variables for output control
int pulltime = 1000;
int contctrl, contstat = 0;  //1 = out 5 high 2 = out 6 high 3 = both high
unsigned long conttimer1, conttimer2, conttimer3, Pretimer, Pretimer1, overtriptimer, undertriptimer, mainconttimer = 0;
uint16_t pwmfreq = 15000;  //pwm frequency

int pwmcurmax = 200;    //Max current to be shown with pwm
int pwmcurmid = 50;     //Mid point for pwm dutycycle based on current
int16_t pwmcurmin = 0;  //DONOT fill in, calculated later based on other values


//variables for VE driect bus comms
const char* myStrings[] = { "V", "14674", "I", "0", "CE", "-1", "SOC", "800", "TTG", "-1", "Alarm", "OFF", "Relay", "OFF", "AR", "0", "BMV", "600S", "FW", "212", "H1", "-3", "H2", "-3", "H3", "0", "H4", "0", "H5", "0", "H6", "-7", "H7", "13180", "H8", "14774", "H9", "137", "H10", "0", "H11", "0", "H12", "0" };

//variables for VE can
uint16_t chargevoltage = 49100;  //max charge voltage in mv
int chargecurrent;
uint16_t disvoltage = 42000;  // max discharge voltage in mv
int discurrent;
uint16_t SOH = 100;  // SOH place holder

unsigned char alarm[4], warning[4] = { 0, 0, 0, 0 };
unsigned char mes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned char bmsname[8] = { 'S', 'I', 'M', 'P', ' ', 'B', 'M', 'S' };
unsigned char bmsmanu[8] = { 'S', 'I', 'M', 'P', ' ', 'E', 'C', 'O' };
long unsigned int rxId;
unsigned char len = 0;
char msgString[128];  // Array to store serial string
uint32_t inbox;
signed long CANmilliamps;
signed long voltage1, voltage2, voltage3 = 0;  //mV only with ISAscale sensor
//struct can_frame canMsg;
//MCP2515 CAN1(10); //set CS pin for can controlelr


//variables for current calulation
int value;
float currentact, RawCur;
float ampsecond;
unsigned long lasttime;
unsigned long looptime, looptime1, UnderTime, OverTime, cleartime, chargertimer = 0;  //ms
int currentsense = 14;
int sensor = 1;

//running average
const int RunningAverageCount = 16;
float RunningAverageBuffer[RunningAverageCount];
int NextRunningAverage;

//Variables for SOC calc
int SOC = 100;  //State of Charge
int SOCset = 0;
int SOCreset = 0;
int SOCtest = 0;
int SOCmem = 0;

///charger variables
int maxac1 = 16;           //Shore power 16A per charger
int maxac2 = 10;           //Generator Charging
int chargerid1 = 0x618;    //bulk chargers
int chargerid2 = 0x638;    //finishing charger
float chargerendbulk = 0;  //V before Charge Voltage to turn off the bulk charger/s
float chargerend = 0;      //V before Charge Voltage to turn off the finishing charger/s
int chargertoggle = 0;
int ncharger = 1;  // number of chargers

//AC current control
volatile uint32_t pilottimer = 0;
volatile uint16_t timehigh, duration = 0;
volatile uint16_t accurlim = 0;
volatile int dutycycle = 0;
uint16_t chargerpower = 0;
bool CPdebug = 0;

//variables
int outputstate = 0;
int incomingByte = 0;
int storagemode = 0;
int x = 0;
int balancecells;
int cellspresent = 0;
int Charged = 0;

//VW BMS CAN variables////////////
int controlid = 0x0BA;
int moduleidstart = 0x1CC;

//Serial Expansion Variables///
int SerialID = 0;      //ID assigned over serialbus
int SerialSlaves = 0;  //number of slaves present

//Balance testing
int balinit = 0;
int balon = 0;
int balcycle = 0;

//Debugging modes//////////////////
int debug = 1;
int gaugedebug = 0;
int inputcheck = 0;   //read digital inputs
int outputcheck = 0;  //check outputs
int candebug = 0;     //view can frames
int debugCur = 0;
int CSVdebug = 0;
int menuload = q_exit_menu;
int debugdigits = 2;  //amount of digits behind decimal for voltage reading

ADC* adc = new ADC();  // adc object

void loadSettings() {
  Logger::console("Resetting to factory defaults");
  settings.version = EEPROM_VERSION;
  settings.checksum = 2;
  settings.canSpeed = 500000;
  settings.batteryID = 0x01;  //in the future should be 0xFF to force it to ask for an address
  settings.OverVSetpoint = 4.2f;
  settings.UnderVSetpoint = 3.0f;
  settings.ChargeVsetpoint = 4.1f;
  settings.ChargeHys = 0.2f;  // voltage drop required for charger to kick back on
  settings.WarnOff = 0.1f;    //voltage offset to raise a warning
  settings.DischVsetpoint = 3.2f;
  settings.CellGap = 0.2f;  //max delta between high and low cell
  settings.OverTSetpoint = 65.0f;
  settings.UnderTSetpoint = -10.0f;
  settings.ChargeTSetpoint = 0.0f;
  settings.DisTSetpoint = 40.0f;
  settings.WarnToff = 5.0f;   //temp offset before raising warning
  settings.IgnoreTemp = 0;    // 0 - use both sensors, 1 or 2 only use that sensor
  settings.IgnoreVolt = 0.5;  //
  settings.balanceVoltage = 3.9f;
  settings.balanceHyst = 0.04f;
  settings.logLevel = 2;
  settings.CAP = 100;               //battery size in Ah
  settings.Pstrings = 1;            // strings in parallel used to divide voltage of pack
  settings.Scells = 12;             //Cells in series
  settings.StoreVsetpoint = 3.8;    // V storage mode charge max
  settings.discurrentmax = 300;     // max discharge current in 0.1A
  settings.DisTaper = 0.3f;         //V offset to bring in discharge taper to Zero Amps at settings.DischVsetpoint
  settings.chargecurrentmax = 300;  //max charge current in 0.1A
  settings.chargecurrentend = 50;   //end charge current in 0.1A
  settings.socvolt[0] = 3100;       //Voltage and SOC curve for voltage based SOC calc
  settings.socvolt[1] = 10;         //Voltage and SOC curve for voltage based SOC calc
  settings.socvolt[2] = 4100;       //Voltage and SOC curve for voltage based SOC calc
  settings.socvolt[3] = 90;         //Voltage and SOC curve for voltage based SOC calc
  settings.invertcur = 0;           //Invert current sensor direction
  settings.cursens = 2;
  settings.curcan = LemCAB300;
  settings.voltsoc = 0;        //SOC purely voltage based
  settings.Pretime = 5000;     //ms of precharge time
  settings.conthold = 50;      //holding duty cycle for contactor 0-255
  settings.Precurrent = 1000;  //ma before closing main contator
  settings.convhigh = 58;      // mV/A current sensor high range channel
  settings.convlow = 643;      // mV/A current sensor low range channel
  settings.changecur = 20000;  //mA change overpoint
  settings.offset1 = 1750;     //mV mid point of channel 1
  settings.offset2 = 1750;     //mV mid point of channel 2
  settings.gaugelow = 50;      //empty fuel gauge pwm
  settings.gaugehigh = 255;    //full fuel gauge pwm
  settings.ESSmode = 0;        //activate ESS mode
  settings.ncur = 1;           //number of multiples to use for current measurement
  settings.chargertype = 2;    // 1 - Brusa NLG5xx 2 - Volt charger 0 -No Charger
  settings.chargerspd = 100;   //ms per message
  settings.UnderDur = 5000;    //ms of allowed undervoltage before throwing open stopping discharge.
  settings.CurDead = 5;        // mV of dead band on current sensor
  settings.ChargerDirect = 1;  //1 - charger is always connected to HV battery // 0 - Charger is behind the contactors
  settings.DeltaVolt = 0.5;    //V of allowable difference between measurements
  settings.tripcont = 1;       //in ESSmode 1 - Main contactor function, 0 - Trip function
  settings.triptime = 500;     //mS of delay before counting over or undervoltage
}

CAN_message_t msg;
CAN_message_t inMsg;
#ifndef USING_TEENSY4
CAN_filter_t filter;
#else
void myCallback() {
  SERIALCONSOLE.println("FEED THE DOG SOON, OR RESET!");
}
#endif

uint32_t lastUpdate;


void setup() {
  delay(4000);  //just for easy debugging. It takes a few seconds for USB to come up properly on most OS's
  pinMode(ACUR1, INPUT);
  pinMode(ACUR2, INPUT);
  pinMode(IN1, INPUT);
  pinMode(IN2, INPUT);
  pinMode(IN3, INPUT);
  pinMode(IN4, INPUT);
  pinMode(OUT1, OUTPUT);  // drive contactor
  digitalWrite(OUT1, LOW);
  pinMode(OUT2, OUTPUT);  // precharge
  digitalWrite(OUT2, LOW);
  pinMode(OUT3, OUTPUT);  // charge relay
  digitalWrite(OUT3, LOW);
  pinMode(OUT4, OUTPUT);  // Negative contactor
  digitalWrite(OUT4, LOW);
  pinMode(OUT5, OUTPUT);  // pwm driver output
  digitalWrite(OUT5, LOW);
  pinMode(OUT6, OUTPUT);  // pwm driver output
  digitalWrite(OUT6, LOW);
  pinMode(OUT7, OUTPUT);  // pwm driver output
  digitalWrite(OUT7, LOW);
  pinMode(OUT8, OUTPUT);  // pwm driver output
  digitalWrite(OUT8, LOW);
  pinMode(led, OUTPUT);

  analogWriteFrequency(OUT5, pwmfreq);
  analogWriteFrequency(OUT6, pwmfreq);
  analogWriteFrequency(OUT7, pwmfreq);
  analogWriteFrequency(OUT8, pwmfreq);

#ifdef USING_TEENSY4
  Can0.begin();
  Can0.setBaudRate(500000);

  Can0.mailboxStatus();
  //set filters for standard 11 bits ID on mailboxes 0 to 8
  for (int i = 0; i < 8; i++) {
    Can0.setMB(static_cast<FLEXCAN_MAILBOX>(i), RX, STD);
  }
  //set filters for extended 29 bits ID on mailboxes 9 to 13
  for (int i = 9; i < 13; i++) {
    Can0.setMB(static_cast<FLEXCAN_MAILBOX>(i), RX, EXT);
  }
  // MB 14 and 15 are TX

  Can0.mailboxStatus();
#else
  Can0.begin(500000);

  //set filters for standard
  for (int i = 0; i < 8; i++) {
    //Can0.getFilter(filter, i);
    filter.flags.extended = 0;
    Can0.setFilter(filter, i);
  }
  //set filters for extended
  for (int i = 9; i < 13; i++) {
    //Can0.getFilter(filter, i);
    filter.flags.extended = 1;
    Can0.setFilter(filter, i);
  }
#endif  //USING_TEENSY4

  //if using enable pins on a transceiver they need to be set on


  adc->adc0->setAveraging(16);   // set number of averages
  adc->adc0->setResolution(16);  // set bits of resolution
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
  adc->adc0->startContinuous(ACUR1);


  SERIALCONSOLE.begin(115200);
  SERIALCONSOLE.println("Starting up!");
  SERIALCONSOLE.println("SimpBMS V2 VW");

  Serial2.begin(115200);

  // Display reason the Teensy was last reset
  SERIALCONSOLE.println();
  SERIALCONSOLE.println("Reason for last Reset: ");
#ifdef USING_TEENSY4
  // Reset cause regisgter not avialable on Teensy 4
  SERIALCONSOLE.println("Reset cause regisgter not avialable on Teensy 4");
#else
  if (RCM_SRS1 & RCM_SRS1_SACKERR) SERIALCONSOLE.println("Stop Mode Acknowledge Error Reset");
  if (RCM_SRS1 & RCM_SRS1_MDM_AP) SERIALCONSOLE.println("MDM-AP Reset");
  if (RCM_SRS1 & RCM_SRS1_SW) SERIALCONSOLE.println("Software Reset");  // reboot with SCB_AIRCR = 0x05FA0004
  if (RCM_SRS1 & RCM_SRS1_LOCKUP) SERIALCONSOLE.println("Core Lockup Event Reset");
  if (RCM_SRS0 & RCM_SRS0_POR) SERIALCONSOLE.println("Power-on Reset");        // removed / applied power
  if (RCM_SRS0 & RCM_SRS0_PIN) SERIALCONSOLE.println("External Pin Reset");    // Reboot with software download
  if (RCM_SRS0 & RCM_SRS0_WDOG) SERIALCONSOLE.println("Watchdog(COP) Reset");  // WDT timed out
  if (RCM_SRS0 & RCM_SRS0_LOC) SERIALCONSOLE.println("Loss of External Clock Reset");
  if (RCM_SRS0 & RCM_SRS0_LOL) SERIALCONSOLE.println("Loss of Lock in PLL Reset");
  if (RCM_SRS0 & RCM_SRS0_LVD) SERIALCONSOLE.println("Low-voltage Detect Reset");
#endif
  SERIALCONSOLE.println();
  ///////////////////

#ifdef USING_TEENSY4
  WDT_timings_t config;
  config.trigger = 10;  /* in seconds, 0->128 */
  config.timeout = 20; /* in seconds, 0->128 */
  config.callback = myCallback;
  wdt.begin(config);
#else
  // enable WDT
  noInterrupts();                  // don't allow interrupts while setting up WDOG
  WDOG_UNLOCK = WDOG_UNLOCK_SEQ1;  // unlock access to WDOG registers
  WDOG_UNLOCK = WDOG_UNLOCK_SEQ2;
  delayMicroseconds(1);  // Need to wait a bit..

  WDOG_TOVALH = 0x1000;
  WDOG_TOVALL = 0x0000;
  WDOG_PRESC = 0;
  WDOG_STCTRLH |= WDOG_STCTRLH_ALLOWUPDATE | WDOG_STCTRLH_WDOGEN | WDOG_STCTRLH_WAITEN | WDOG_STCTRLH_STOPEN | WDOG_STCTRLH_CLKSRC;
  interrupts();
#endif  //USING_TEENSY4
  /////////////////
  SERIALBMS.begin(115200);
  //SERIALBMS.begin(612500); //Tesla serial bus
  //VE.begin(19200); //Victron VE direct bus
#if defined(__arm__) && defined(__SAM3X8E__)
  serialSpecialInit(USART0, 612500);  //required for Due based boards as the stock core files don't support 612500 baud.
#endif

  SERIALCONSOLE.println("Started serial interface to BMS.");

  EEPROM.get(0, settings);
  if (settings.version != EEPROM_VERSION) {
    SERIALCONSOLE.println();
    loadSettings();
  }

  Logger::setLoglevel(Logger::Off);  //Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4

  lastUpdate = 0;
  digitalWrite(led, HIGH);
  bms.setPstrings(settings.Pstrings);
  bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
  bms.setBalanceHyst(settings.balanceHyst);

  //SOC recovery//

  SOC = (EEPROM.read(1000));
  if (settings.voltsoc == 1) {
    SOCmem = 0;
  } else {
    if (SOC > 100) {
      SOCmem = 0;
    } else if (SOC > 1) {
      //SOCmem = 1;
    }
  }

  SERIALCONSOLE.println("Recovery SOC: ");
  SERIALCONSOLE.print(SOC);


  ////Calculate fixed numbers
  pwmcurmin = (pwmcurmid / 50 * pwmcurmax * -1);
  ////
  //if (settings.Serialexp == 1)
  //{
  //  delay(300);//wait for all other boards to boot
  //Serialslaveinit();
  // }


  ///precharge timer kickers
  Pretimer = millis();
  Pretimer1 = millis();

  // setup interrupts
  //RISING/HIGH/CHANGE/LOW/FALLING
  attachInterrupt(IN4, isrCP, CHANGE);  // attach BUTTON 1 interrupt handler [ pin# 7 ]

// TEESNY4 has no low voltage detector
#ifndef USING_TEENSY4
  PMC_LVDSC1 = PMC_LVDSC1_LVDV(1);                     // enable hi v
  PMC_LVDSC2 = PMC_LVDSC2_LVWIE | PMC_LVDSC2_LVWV(3);  // 2.92-3.08v
  attachInterruptVector(IRQ_LOW_VOLTAGE, low_voltage_isr);
  NVIC_ENABLE_IRQ(IRQ_LOW_VOLTAGE);
#endif
  cleartime = millis();
}

void loop() {
  #ifdef USING_TEENSY4
  canread();
  #else
  while (Can0.available()) {
    canread();
  }
  #endif

  if (SERIALCONSOLE.available() > 0) {
    menu();
  }

  if (outputcheck != 1) {
    contcon();
    if (settings.ESSmode == 1) {
      if (bmsstatus != Error && bmsstatus != Boot) {
        contctrl = contctrl | 4;  //turn on negative contactor

        if (settings.tripcont != 0) {
          if (bms.getLowCellVolt() > settings.UnderVSetpoint && bms.getHighCellVolt() < settings.OverVSetpoint) {
            if (digitalRead(OUT2) == LOW && digitalRead(OUT4) == LOW) {
              mainconttimer = millis();
              digitalWrite(OUT4, HIGH);  //Precharge start
              SERIALCONSOLE.println();
              SERIALCONSOLE.println("Precharge!!!");
              SERIALCONSOLE.println(mainconttimer);
              SERIALCONSOLE.println();
            }
            if (mainconttimer + settings.Pretime < millis() && digitalRead(OUT2) == LOW && abs(currentact) < settings.Precurrent) {
              digitalWrite(OUT2, HIGH);  //turn on contactor
              contctrl = contctrl | 2;   //turn on contactor
              SERIALCONSOLE.println();
              SERIALCONSOLE.println("Main On!!!");
              SERIALCONSOLE.println();
              mainconttimer = millis() + settings.Pretime;
            }
            if (mainconttimer + settings.Pretime + 1000 < millis()) {
              digitalWrite(OUT4, LOW);  //ensure precharge is low
            }
          } else {
            digitalWrite(OUT4, LOW);  //ensure precharge is low
            mainconttimer = 0;
          }
        }
        if (digitalRead(IN1) == LOW)  //Key OFF
        {
          if (storagemode == 1) {
            storagemode = 0;
          }
        } else {
          if (storagemode == 0) {
            storagemode = 1;
          }
        }
        if (bms.getHighCellVolt() > settings.balanceVoltage && bms.getHighCellVolt() > bms.getLowCellVolt() + settings.balanceHyst) {
          //bms.balanceCells(1);
          balancecells = 1;
        } else {
          balancecells = 0;
        }

        //Pretimer + settings.Pretime > millis();

        if (storagemode == 1) {
          if (bms.getHighCellVolt() > settings.StoreVsetpoint || chargecurrent == 0) {
            digitalWrite(OUT3, LOW);  //turn off charger
            // contctrl = contctrl & 253;
            // Pretimer = millis();
            Charged = 1;
            SOCcharged(2);
          } else {
            if (Charged == 1) {
              if (bms.getHighCellVolt() < (settings.StoreVsetpoint - settings.ChargeHys)) {
                Charged = 0;
                digitalWrite(OUT3, HIGH);  //turn on charger
                /*
                  if (Pretimer + settings.Pretime < millis())
                  {
                  contctrl = contctrl | 2;
                  Pretimer = 0;
                  }
                */
              }
            } else {
              digitalWrite(OUT3, HIGH);  //turn on charger
              /*
                if (Pretimer + settings.Pretime < millis())
                {
                contctrl = contctrl | 2;
                Pretimer = 0;
                }
              */
            }
          }
        } else {
          if (bms.getHighCellVolt() > settings.OverVSetpoint || bms.getHighCellVolt() > settings.ChargeVsetpoint || chargecurrent == 0) {
            if ((millis() - overtriptimer) > settings.triptime) {
              if (digitalRead(OUT3) == 1) {
                SERIALCONSOLE.println();
                SERIALCONSOLE.println("Over Voltage Trip");
                digitalWrite(OUT3, LOW);  //turn off charger
                // contctrl = contctrl & 253;
                //Pretimer = millis();
                Charged = 1;
                SOCcharged(2);
              }
            }
          } else {
            overtriptimer = millis();
            if (Charged == 1) {

              if (bms.getHighCellVolt() < (settings.ChargeVsetpoint - settings.ChargeHys)) {
                if (digitalRead(OUT3) == 0) {
                  SERIALCONSOLE.println();
                  SERIALCONSOLE.println("Reset Over Voltage Trip Not Charged");
                  Charged = 0;
                  digitalWrite(OUT3, HIGH);  //turn on charger
                }
                /*
                  if (Pretimer + settings.Pretime < millis())
                  {
                  // SERIALCONSOLE.println();
                  //SERIALCONSOLE.print(Pretimer);
                  contctrl = contctrl | 2;
                  }*/
              }

            } else {
              if (digitalRead(OUT3) == 0) {
                SERIALCONSOLE.println();
                SERIALCONSOLE.println("Reset Over Voltage Trip Not Charged");
                digitalWrite(OUT3, HIGH);  //turn on charger
              }
              /*
                if (Pretimer + settings.Pretime < millis())
                {
                // SERIALCONSOLE.println();
                //SERIALCONSOLE.print(Pretimer);
                contctrl = contctrl | 2;
                }*/
            }
          }
        }

        if (bms.getLowCellVolt() < settings.UnderVSetpoint || bms.getLowCellVolt() < settings.DischVsetpoint) {
          if (digitalRead(OUT1) == 1) {

            if ((millis() - undertriptimer) > settings.triptime) {
              SERIALCONSOLE.println();
              SERIALCONSOLE.println("Under Voltage Trip");
              digitalWrite(OUT1, LOW);  //turn off discharge
              // contctrl = contctrl & 254;
              // Pretimer1 = millis();
            }
          }
        } else {
          undertriptimer = millis();

          if (bms.getLowCellVolt() > settings.DischVsetpoint + settings.DischHys) {
            if (digitalRead(OUT1) == 0) {
              SERIALCONSOLE.println();
              SERIALCONSOLE.println("Reset Under Voltage Trip");
              digitalWrite(OUT1, HIGH);  //turn on discharge
            }
            /*
              if (Pretimer1 + settings.Pretime < millis())
              {
              contctrl = contctrl | 1;
              }*/
          }
        }

        if (SOCset == 1) {
          if (settings.tripcont == 0) {
            if (bms.getLowCellVolt() < settings.UnderVSetpoint || bms.getHighCellVolt() > settings.OverVSetpoint || bms.getHighTemperature() > settings.OverTSetpoint) {
              digitalWrite(OUT2, HIGH);  //trip breaker
              bmsstatus = Error;
            } else {
              digitalWrite(OUT2, LOW);  //trip breaker
            }
          } else {
            if (bms.getLowCellVolt() < settings.UnderVSetpoint || bms.getHighCellVolt() > settings.OverVSetpoint || bms.getHighTemperature() > settings.OverTSetpoint) {
              digitalWrite(OUT2, LOW);    //turn off contactor
              contctrl = contctrl & 253;  //turn off contactor
              digitalWrite(OUT4, LOW);    //ensure precharge is low
              bmsstatus = Error;
            }
          }
        }
      } else {
        //digitalWrite(OUT2, HIGH);//trip breaker
        Discharge = 0;
        digitalWrite(OUT4, LOW);
        digitalWrite(OUT3, LOW);  //turn off charger
        digitalWrite(OUT2, LOW);
        digitalWrite(OUT1, LOW);  //turn off discharge
        contctrl = 0;             //turn off out 5 and 6

        if (SOCset == 1) {
          if (settings.tripcont == 0) {

            digitalWrite(OUT2, HIGH);  //trip breaker
          } else {
            digitalWrite(OUT2, LOW);  //turn off contactor
            digitalWrite(OUT4, LOW);  //ensure precharge is low
          }

          if (bms.getLowCellVolt() > settings.UnderVSetpoint && bms.getHighCellVolt() < settings.OverVSetpoint && bms.getHighTemperature() < settings.OverTSetpoint && cellspresent == bms.seriescells() && cellspresent == (settings.Scells * settings.Pstrings)) {
            if (ErrorReason == 0) {
              bmsstatus = Ready;
            }
          }
        }
      }
      //pwmcomms();
    } else {
      switch (bmsstatus) {
        case (Boot):
          Discharge = 0;
          digitalWrite(OUT4, LOW);
          digitalWrite(OUT3, LOW);  //turn off charger
          digitalWrite(OUT2, LOW);
          digitalWrite(OUT1, LOW);  //turn off discharge
          contctrl = 0;
          bmsstatus = Ready;
          break;

        case (Ready):
          Discharge = 0;
          digitalWrite(OUT4, LOW);
          digitalWrite(OUT3, LOW);  //turn off charger
          digitalWrite(OUT2, LOW);
          digitalWrite(OUT1, LOW);  //turn off discharge
          contctrl = 0;             //turn off out 5 and 6
          if (bms.getHighCellVolt() > settings.balanceVoltage && bms.getHighCellVolt() > bms.getLowCellVolt() + settings.balanceHyst) {
            //bms.balanceCells(1);
            balancecells = 1;
          } else {
            balancecells = 0;
          }
          if (digitalRead(IN3) == HIGH && (bms.getHighCellVolt() < (settings.ChargeVsetpoint - settings.ChargeHys)))  //detect AC present for charging and check not balancing
          {
            if (settings.ChargerDirect == 1) {
              bmsstatus = Charge;
            } else {
              bmsstatus = Precharge;
              Pretimer = millis();
            }
          }
          if (digitalRead(IN1) == HIGH && bms.getLowCellVolt() > settings.DischVsetpoint)  //detect Key ON
          {
            bmsstatus = Precharge;
            Pretimer = millis();
          }

          break;

        case (Precharge):
          Discharge = 0;
          Prechargecon();
          break;


        case (Drive):
          Discharge = 1;
          if (digitalRead(IN1) == LOW)  //Key OFF
          {
            bmsstatus = Ready;
          }
          if (digitalRead(IN3) == HIGH && (bms.getHighCellVolt() < (settings.ChargeVsetpoint - settings.ChargeHys)))  //detect AC present for charging and check not balancing
          {
            bmsstatus = Charge;
          }

          break;

        case (Charge):
          Discharge = 0;
          digitalWrite(OUT3, HIGH);  //enable charger
          if (bms.getHighCellVolt() > settings.balanceVoltage) {
            //bms.balanceCells(1);
            balancecells = 1;
          } else {
            balancecells = 0;
          }
          if (bms.getHighCellVolt() > settings.ChargeVsetpoint) {
            if (bms.getAvgCellVolt() > (settings.ChargeVsetpoint - settings.ChargeHys)) {
              SOCcharged(2);
            } else {
              SOCcharged(1);
            }
            digitalWrite(OUT3, LOW);  //turn off charger
            bmsstatus = Ready;
          }
          if (digitalRead(IN3) == LOW)  //detect AC not present for charging
          {
            bmsstatus = Ready;
          }
          break;

        case (Error):
          Discharge = 0;
          digitalWrite(OUT4, LOW);
          digitalWrite(OUT3, LOW);  //turn off charger
          digitalWrite(OUT2, LOW);
          digitalWrite(OUT1, LOW);  //turn off discharge
          contctrl = 0;             //turn off out 5 and 6

          if (bms.getLowCellVolt() >= settings.UnderVSetpoint && bms.getHighCellVolt() <= settings.OverVSetpoint && digitalRead(IN1) == LOW) {
            bmsstatus = Ready;
          }

          break;
      }
    }
    if (settings.cursens == Analoguedual || settings.cursens == Analoguesing) {
      getcurrent();
    }
  }

  if (millis() - looptime > 500) {
    looptime = millis();
    bms.getAllVoltTemp();
    //UV  check
    if (SOCset == 1 && balancecells == 1) {
      bms.balanceCells(0);  //1 is debug
    }

    if (settings.ESSmode == 1) {
      if (SOCset != 0) {
        if (bms.getLowCellVolt() < settings.UnderVSetpoint || bms.getHighCellVolt() < settings.UnderVSetpoint) {
          if (debug != 0) {
            SERIALCONSOLE.println("  ");
            SERIALCONSOLE.print("   !!! Undervoltage Fault !!!");
            SERIALCONSOLE.println("  ");
          }
          bmsstatus = Error;
          ErrorReason = ErrorReason | 0x01;
        } else {
          ErrorReason = ErrorReason & ~0x01;
        }
      }
    } else  //In 'vehicle' mode
    {
      if (SOCset != 0) {
        if (bms.getLowCellVolt() < settings.UnderVSetpoint) {
          if (UnderTime < millis())  //check is last time not undervoltage is longer thatn UnderDur ago
          {
            bmsstatus = Error;
          }
        } else {
          UnderTime = millis() + settings.triptime;
        }

        if (bms.getHighCellVolt() < settings.UnderVSetpoint || bms.getHighTemperature() > settings.OverTSetpoint) {
          bmsstatus = Error;
        }

        if (bms.getHighCellVolt() > settings.OverVSetpoint) {
          if (OverTime < millis())  //check is last time not undervoltage is longer thatn UnderDur ago
          {
            bmsstatus = Error;
          }
        } else {
          OverTime = millis() + settings.triptime;
        }
      }
    }

    if (debug != 0) {
      printbmsstat();
      bms.printPackDetails(debugdigits);
    }
    if (CSVdebug != 0) {
      bms.printAllCSV(millis(), currentact, SOC);
    }
    if (inputcheck != 0) {
      inputdebug();
    }

    if (outputcheck != 0) {
      outputdebug();
    } else {
      gaugeupdate();
    }

    updateSOC();
    currentlimit();
    VEcan();

    sendcommand();

    if (cellspresent == 0 && SOCset == 1) {
      cellspresent = bms.seriescells();
      bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
    } else {
      if (cellspresent != bms.seriescells() || cellspresent != (settings.Scells * settings.Pstrings))  //detect a fault in cells detected
      {
        if (debug != 0) {
          SERIALCONSOLE.println("  ");
          SERIALCONSOLE.print("   !!! Series Cells Fault !!!");
          SERIALCONSOLE.println("  ");
        }
        bmsstatus = Error;
        ErrorReason = ErrorReason | 0x04;
      } else {
        ErrorReason = ErrorReason & ~0x04;
      }
    }
    alarmupdate();
    if (CSVdebug != 1) {
      dashupdate();
    }

    if (bmsstatus == Error && ErrorReason == 0) {
      bmsstatus = Boot;
    }

    resetwdog();
  }

  if (millis() - cleartime > 20000) {
    if (bms.checkcomms()) {
      //no missing modules
      /*
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print(" ALL OK NO MODULE MISSING :) ");
        SERIALCONSOLE.println("  ");
      */
      ErrorReason = ErrorReason & ~0x08;
      if (bmsstatus == Error && ErrorReason == 0) {
        bmsstatus = Boot;
      }
    } else {
      //missing module
      if (debug != 0) {
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("   !!! MODULE MISSING !!!");
        SERIALCONSOLE.println("  ");
      }
      bmsstatus = Error;
      ErrorReason = ErrorReason | 0x08;
    }
    //bms.clearmodules(); // Not functional
    cleartime = millis();
  }

  if (millis() - looptime1 > (unsigned int)settings.chargerspd) {
    looptime1 = millis();
    if (settings.ESSmode == 1) {
      chargercomms();
    } else {
      if (bmsstatus == Charge) {
        chargercomms();
      }
    }
  }
}

void alarmupdate() {
  alarm[0] = 0x00;
  if (settings.OverVSetpoint < bms.getHighCellVolt()) {
    alarm[0] = 0x04;
  }
  if (bms.getLowCellVolt() < settings.UnderVSetpoint) {
    alarm[0] |= 0x10;
  }
  if (bms.getHighTemperature() > settings.OverTSetpoint) {
    alarm[0] |= 0x40;
  }
  alarm[1] = 0;
  if (bms.getLowTemperature() < settings.UnderTSetpoint) {
    alarm[1] = 0x01;
  }
  alarm[3] = 0;
  if ((bms.getHighCellVolt() - bms.getLowCellVolt()) > settings.CellGap) {
    alarm[3] = 0x01;
  }

  ///warnings///
  warning[0] = 0;

  if (bms.getHighCellVolt() > (settings.OverVSetpoint - settings.WarnOff)) {
    warning[0] = 0x04;
  }
  if (bms.getLowCellVolt() < (settings.UnderVSetpoint + settings.WarnOff)) {
    warning[0] |= 0x10;
  }

  if (bms.getHighTemperature() > (settings.OverTSetpoint - settings.WarnToff)) {
    warning[0] |= 0x40;
  }
  warning[1] = 0;
  if (bms.getLowTemperature() < (settings.UnderTSetpoint + settings.WarnToff)) {
    warning[1] = 0x01;
  }
}

void gaugeupdate() {
  if (gaugedebug == 1) {
    SOCtest = SOCtest + 10;
    if (SOCtest > 1000) {
      SOCtest = 0;
    }
    analogWrite(OUT8, map(SOCtest * 0.1, 0, 100, settings.gaugelow, settings.gaugehigh));

    SERIALCONSOLE.println("  ");
    SERIALCONSOLE.print("SOC : ");
    SERIALCONSOLE.print(SOCtest * 0.1);
    SERIALCONSOLE.print("  fuel pwm : ");
    SERIALCONSOLE.print(map(SOCtest * 0.1, 0, 100, settings.gaugelow, settings.gaugehigh));
    SERIALCONSOLE.println("  ");
  }
  if (gaugedebug == 2) {
    SOCtest = 0;
    analogWrite(OUT8, map(SOCtest * 0.1, 0, 100, settings.gaugelow, settings.gaugehigh));
  }
  if (gaugedebug == 3) {
    SOCtest = 1000;
    analogWrite(OUT8, map(SOCtest * 0.1, 0, 100, settings.gaugelow, settings.gaugehigh));
  }
  if (gaugedebug == 0) {
    analogWrite(OUT8, map(SOC, 0, 100, settings.gaugelow, settings.gaugehigh));
  }
}

void printbmsstat() {
  SERIALCONSOLE.println();
  SERIALCONSOLE.println();
  SERIALCONSOLE.println();
  SERIALCONSOLE.print("BMS Status : ");
  if (settings.ESSmode == 1) {
    SERIALCONSOLE.print("ESS Mode ");

    if (bms.getLowCellVolt() < settings.UnderVSetpoint) {
      SERIALCONSOLE.print(": UnderVoltage ");
    }
    if (bms.getHighCellVolt() > settings.OverVSetpoint) {
      SERIALCONSOLE.print(": OverVoltage ");
    }
    if ((bms.getHighCellVolt() - bms.getLowCellVolt()) > settings.CellGap) {
      SERIALCONSOLE.print(": Cell Imbalance ");
    }
    if (bms.getAvgTemperature() > settings.OverTSetpoint) {
      SERIALCONSOLE.print(": Over Temp ");
    }
    if (bms.getAvgTemperature() < settings.UnderTSetpoint) {
      SERIALCONSOLE.print(": Under Temp ");
    }
    if (storagemode == 1) {
      if (bms.getLowCellVolt() > settings.StoreVsetpoint) {
        SERIALCONSOLE.print(": OverVoltage Storage ");
        SERIALCONSOLE.print(": UNhappy:");
      } else {
        SERIALCONSOLE.print(": Happy ");
      }
    } else {
      if (bms.getLowCellVolt() > settings.UnderVSetpoint && bms.getHighCellVolt() < settings.OverVSetpoint) {

        if (bmsstatus == Error) {
          SERIALCONSOLE.print(": UNhappy:");
        } else {
          SERIALCONSOLE.print(": Happy ");
        }
      }
    }
    SERIALCONSOLE.print("ErrSt: ");
    SERIALCONSOLE.print(ErrorReason);
  } else {
    SERIALCONSOLE.print(bmsstatus);
    switch (bmsstatus) {
      case (Boot):
        SERIALCONSOLE.print(" Boot ");
        break;

      case (Ready):
        SERIALCONSOLE.print(" Ready ");
        break;

      case (Precharge):
        SERIALCONSOLE.print(" Precharge ");
        break;

      case (Drive):
        SERIALCONSOLE.print(" Drive ");
        break;

      case (Charge):
        SERIALCONSOLE.print(" Charge ");
        break;

      case (Error):
        SERIALCONSOLE.print(" Error ");
        break;
    }
  }
  SERIALCONSOLE.print("  ");
  if (digitalRead(IN3) == HIGH) {
    SERIALCONSOLE.print("| AC Present |");
  }
  if (digitalRead(IN1) == HIGH) {
    SERIALCONSOLE.print("| Key ON |");
  }
  if (balancecells == 1) {
    SERIALCONSOLE.print("|Balancing Active");
  }
  SERIALCONSOLE.print("  ");
  SERIALCONSOLE.print(cellspresent);
  SERIALCONSOLE.println();
  SERIALCONSOLE.print("Out:");
  SERIALCONSOLE.print(digitalRead(OUT1));
  SERIALCONSOLE.print(digitalRead(OUT2));
  SERIALCONSOLE.print(digitalRead(OUT3));
  SERIALCONSOLE.print(digitalRead(OUT4));
  SERIALCONSOLE.print(" Cont:");
  if ((contstat & 1) == 1) {
    SERIALCONSOLE.print("1");
  } else {
    SERIALCONSOLE.print("0");
  }
  if ((contstat & 2) == 2) {
    SERIALCONSOLE.print("1");
  } else {
    SERIALCONSOLE.print("0");
  }
  if ((contstat & 4) == 4) {
    SERIALCONSOLE.print("1");
  } else {
    SERIALCONSOLE.print("0");
  }
  if ((contstat & 8) == 8) {
    SERIALCONSOLE.print("1");
  } else {
    SERIALCONSOLE.print("0");
  }
  SERIALCONSOLE.print(" In:");
  SERIALCONSOLE.print(digitalRead(IN1));
  SERIALCONSOLE.print(digitalRead(IN2));
  SERIALCONSOLE.print(digitalRead(IN3));
  SERIALCONSOLE.print(digitalRead(IN4));
}


void getcurrent() {
  if (settings.cursens == Analoguedual || settings.cursens == Analoguesing) {
    if (settings.cursens == Analoguedual) {
      if (currentact < settings.changecur && currentact > (settings.changecur * -1)) {
        sensor = 1;
        adc->startContinuous(ACUR1);
      } else {
        sensor = 2;
        adc->adc0->startContinuous(ACUR2);
      }
    } else {
      sensor = 1;
      adc->adc0->startContinuous(ACUR1);
    }
    if (sensor == 1) {
      if (debugCur != 0) {
        SERIALCONSOLE.println();
        if (settings.cursens == Analoguedual) {
          SERIALCONSOLE.print("Low Range: ");
        } else {
          SERIALCONSOLE.print("Single In: ");
        }
        SERIALCONSOLE.print("Value ADC0: ");
      }
      value = (uint16_t)adc->adc0->analogReadContinuous();  // the unsigned is necessary for 16 bits, otherwise values larger than 3.3/2 V are negative!
      if (debugCur != 0) {
        SERIALCONSOLE.print(value * 3300 / adc->adc0->getMaxValue());  //- settings.offset1)
        SERIALCONSOLE.print(" ");
        SERIALCONSOLE.print(settings.offset1);
      }
      RawCur = int16_t((value * 3300 / adc->adc0->getMaxValue()) - settings.offset1) / (settings.convlow * 0.00001);

      if (abs((int16_t(value * 3300 / adc->adc0->getMaxValue()) - settings.offset1)) < settings.CurDead) {
        RawCur = 0;
      }
      if (debugCur != 0) {
        SERIALCONSOLE.print("  ");
        SERIALCONSOLE.print(int16_t(value * 3300 / adc->adc0->getMaxValue()) - settings.offset1);
        SERIALCONSOLE.print("  ");
        SERIALCONSOLE.print(RawCur);
        SERIALCONSOLE.print(" mA");
        SERIALCONSOLE.print("  ");
      }
    } else {
      if (debugCur != 0) {
        SERIALCONSOLE.println();
        SERIALCONSOLE.print("High Range: ");
        SERIALCONSOLE.print("Value ADC0: ");
      }
      value = (uint16_t)adc->adc0->analogReadContinuous();  // the unsigned is necessary for 16 bits, otherwise values larger than 3.3/2 V are negative!
      if (debugCur != 0) {
        SERIALCONSOLE.print(value * 3300 / adc->adc0->getMaxValue());  //- settings.offset2)
        SERIALCONSOLE.print("  ");
        SERIALCONSOLE.print(settings.offset2);
      }
      RawCur = int16_t((value * 3300 / adc->adc0->getMaxValue()) - settings.offset2) / (settings.convhigh * 0.00001);
      if (value < 100 || value > (int)(adc->adc0->getMaxValue() - 100)) {
        RawCur = 0;
      }
      if (debugCur != 0) {
        SERIALCONSOLE.print("  ");
        SERIALCONSOLE.print((float(value * 3300 / adc->adc0->getMaxValue()) - settings.offset2));
        SERIALCONSOLE.print("  ");
        SERIALCONSOLE.print(RawCur);
        SERIALCONSOLE.print("mA");
        SERIALCONSOLE.print("  ");
      }
    }
  }

  if (settings.invertcur == 1) {
    RawCur = RawCur * -1;
  }

  lowpassFilter.input(RawCur);
  if (debugCur != 0) {
    SERIALCONSOLE.print(lowpassFilter.output());
    SERIALCONSOLE.print(" | ");
    SERIALCONSOLE.print(settings.changecur);
    SERIALCONSOLE.print(" | ");
  }

  currentact = lowpassFilter.output();

  if (debugCur != 0) {
    SERIALCONSOLE.print(currentact);
    SERIALCONSOLE.print("mA  ");
  }

  if (settings.cursens == Analoguedual) {
    if (sensor == 1) {
      if (currentact > 500 || currentact < -500) {
        ampsecond = ampsecond + ((currentact * (millis() - lasttime) / 1000) / 1000);
        lasttime = millis();
      } else {
        lasttime = millis();
      }
    }
    if (sensor == 2) {
      if (currentact > settings.changecur || currentact < (settings.changecur * -1)) {
        ampsecond = ampsecond + ((currentact * (millis() - lasttime) / 1000) / 1000);
        lasttime = millis();
      } else {
        lasttime = millis();
      }
    }
  } else {
    if (currentact > 500 || currentact < -500) {
      ampsecond = ampsecond + ((currentact * (millis() - lasttime) / 1000) / 1000);
      lasttime = millis();
    } else {
      lasttime = millis();
    }
  }
  currentact = settings.ncur * currentact;
  RawCur = 0;
}



void updateSOC() {
  if (SOCreset == 1) {
    SOC = map(uint16_t(bms.getLowCellVolt() * 1000), settings.socvolt[0], settings.socvolt[2], settings.socvolt[1], settings.socvolt[3]);
    ampsecond = (SOC * settings.CAP * settings.Pstrings * 10) / 0.27777777777778;
    SOCreset = 0;
  }

  if (SOCset == 0 && SOCmem == 0) {
    if (millis() > 9000) {
      bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
    }
    if (millis() > 10000) {
      SOC = map(uint16_t(bms.getLowCellVolt() * 1000), settings.socvolt[0], settings.socvolt[2], settings.socvolt[1], settings.socvolt[3]);

      ampsecond = (SOC * settings.CAP * settings.Pstrings * 10) / 0.27777777777778;
      SOCset = 1;
      if (debug != 0) {
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.println("//////////////////////////////////////// SOC SET ////////////////////////////////////////");
      }
      if (settings.ESSmode == 1) {
        bmsstatus = Ready;
      }
    }
  }

  if (SOCset == 0 && SOCmem == 1) {
    ampsecond = (SOC * settings.CAP * settings.Pstrings * 10) / 0.27777777777778;
    if (millis() > 9000) {
      bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
    }
    if (millis() > 10000) {
      SOCset = 1;
      if (debug != 0) {
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.println("//////////////////////////////////////// SOC SET ////////////////////////////////////////");
      }
      if (settings.ESSmode == 1) {
        bmsstatus = Ready;
      }
    }
  }

  SOC = ((ampsecond * 0.27777777777778) / (settings.CAP * settings.Pstrings * 1000)) * 100;

  if (settings.voltsoc == 1 || settings.cursens == 0) {
    SOC = map(uint16_t(bms.getLowCellVolt() * 1000), settings.socvolt[0], settings.socvolt[2], settings.socvolt[1], settings.socvolt[3]);

    ampsecond = (SOC * settings.CAP * settings.Pstrings * 10) / 0.27777777777778;
  }

  if (SOC >= 100) {
    ampsecond = (settings.CAP * settings.Pstrings * 1000) / 0.27777777777778;  //reset to full, dependant on given capacity. Need to improve with auto correction for capcity.
    SOC = 100;
  }


  if (SOC < 0) {
    SOC = 0;  //reset SOC this way the can messages remain in range for other devices. Ampseconds will keep counting.
  }

  if (debug != 0) {
    if (settings.cursens == Analoguedual) {
      if (sensor == 1) {
        SERIALCONSOLE.print("Low Range ");
      } else {
        SERIALCONSOLE.print("High Range");
      }
    }
    if (settings.cursens == Analoguesing) {
      SERIALCONSOLE.print("Analogue Single ");
    }
    if (settings.cursens == Canbus) {
      SERIALCONSOLE.print("CANbus ");
    }
    SERIALCONSOLE.print("  ");
    SERIALCONSOLE.print(currentact);
    SERIALCONSOLE.print("mA");
    SERIALCONSOLE.print("  ");
    SERIALCONSOLE.print(SOC);
    SERIALCONSOLE.print("% SOC ");
    SERIALCONSOLE.print(ampsecond * 0.27777777777778, 2);
    SERIALCONSOLE.print("mAh");
  }
}
void SOCcharged(int y) {
  if (y == 1) {
    SOC = 95;
    ampsecond = (settings.CAP * settings.Pstrings * 1000) / 0.27777777777778;  //reset to full, dependant on given capacity. Need to improve with auto correction for capcity.
  }
  if (y == 2) {
    SOC = 100;
    ampsecond = (settings.CAP * settings.Pstrings * 1000) / 0.27777777777778;  //reset to full, dependant on given capacity. Need to improve with auto correction for capcity.
  }
}

void Prechargecon() {
  if (digitalRead(IN1) == HIGH || digitalRead(IN3) == HIGH)  //detect Key ON or AC present
  {
    digitalWrite(OUT4, HIGH);  //Negative Contactor Close
    contctrl = 2;
    if (Pretimer + settings.Pretime > millis() || currentact > settings.Precurrent) {
      digitalWrite(OUT2, HIGH);  //precharge
    } else                       //close main contactor
    {
      digitalWrite(OUT1, HIGH);  //Positive Contactor Close
      contctrl = 3;
      if (settings.ChargerDirect == 1) {
        bmsstatus = Drive;
      } else {
        if (digitalRead(IN3) == HIGH) {
          bmsstatus = Charge;
        }
        if (digitalRead(IN1) == HIGH) {
          bmsstatus = Drive;
        }
      }
      digitalWrite(OUT2, LOW);
    }
  } else {
    digitalWrite(OUT1, LOW);
    digitalWrite(OUT2, LOW);
    digitalWrite(OUT4, LOW);
    bmsstatus = Ready;
    contctrl = 0;
  }
}

void contcon() {
  if (contctrl != contstat)  //check for contactor request change
  {
    if ((contctrl & 1) == 0) {
      analogWrite(OUT5, 0);
      contstat = contstat & 254;
    }
    if ((contctrl & 2) == 0) {
      analogWrite(OUT6, 0);
      contstat = contstat & 253;
    }
    if ((contctrl & 4) == 0) {
      analogWrite(OUT7, 0);
      contstat = contstat & 251;
    }


    if ((contctrl & 1) == 1) {
      if ((contstat & 1) != 1) {
        if (conttimer1 == 0) {
          analogWrite(OUT5, 255);
          conttimer1 = millis() + pulltime;
        }
        if (conttimer1 < millis()) {
          analogWrite(OUT5, settings.conthold);
          contstat = contstat | 1;
          conttimer1 = 0;
        }
      }
    }

    if ((contctrl & 2) == 2) {
      if ((contstat & 2) != 2) {
        if (conttimer2 == 0) {
          SERIALCONSOLE.println();
          SERIALCONSOLE.println("pull in OUT6");
          analogWrite(OUT6, 255);
          conttimer2 = millis() + pulltime;
        }
        if (conttimer2 < millis()) {
          analogWrite(OUT6, settings.conthold);
          contstat = contstat | 2;
          conttimer2 = 0;
        }
      }
    }
    if ((contctrl & 4) == 4) {
      if ((contstat & 4) != 4) {
        if (conttimer3 == 0) {
          SERIALCONSOLE.println();
          SERIALCONSOLE.println("pull in OUT7");
          analogWrite(OUT7, 255);
          conttimer3 = millis() + pulltime;
        }
        if (conttimer3 < millis()) {
          analogWrite(OUT7, settings.conthold);
          contstat = contstat | 4;
          conttimer3 = 0;
        }
      }
    }
    /*
       SERIALCONSOLE.print(conttimer);
       SERIALCONSOLE.print("  ");
       SERIALCONSOLE.print(contctrl);
       SERIALCONSOLE.print("  ");
       SERIALCONSOLE.print(contstat);
       SERIALCONSOLE.println("  ");
    */
  }
  if (contctrl == 0) {
    analogWrite(OUT5, 0);
    analogWrite(OUT6, 0);
  }
}

void calcur() {
  adc->adc0->startContinuous(ACUR1);
  sensor = 1;
  x = 0;
  SERIALCONSOLE.print(" Calibrating Current Offset ::::: ");
  while (x < 20) {
    settings.offset1 = settings.offset1 + ((uint16_t)adc->adc0->analogReadContinuous() * 3300 / adc->adc0->getMaxValue());
    SERIALCONSOLE.print(".");
    delay(100);
    x++;
  }
  settings.offset1 = settings.offset1 / 21;
  SERIALCONSOLE.print(settings.offset1);
  SERIALCONSOLE.print(" current offset 1 calibrated ");
  SERIALCONSOLE.println("  ");
  x = 0;
  adc->adc0->startContinuous(ACUR2);
  sensor = 2;
  SERIALCONSOLE.print(" Calibrating Current Offset ::::: ");
  while (x < 20) {
    settings.offset2 = settings.offset2 + ((uint16_t)adc->adc0->analogReadContinuous() * 3300 / adc->adc0->getMaxValue());
    SERIALCONSOLE.print(".");
    delay(100);
    x++;
  }
  settings.offset2 = settings.offset2 / 21;
  SERIALCONSOLE.print(settings.offset2);
  SERIALCONSOLE.print(" current offset 2 calibrated ");
  SERIALCONSOLE.println("  ");
}

void VEcan()  //communication with Victron system over CAN
{
  if (settings.chargertype == Pylon) {
    msg.id = 0x359;
    msg.len = 8;
    msg.buf[0] = 0x00;  //protection to be translated later date
    msg.buf[1] = 0x00;  //protection to be translated later date
    msg.buf[2] = 0x00;  //protection to be translated later date
    msg.buf[3] = 0x00;  //protection to be translated later date
    msg.buf[4] = 0x01;  //number of modules fixed for now
    msg.buf[5] = 0x50;
    msg.buf[6] = 0x4E;
    msg.buf[7] = 0x00;
    Can0.write(msg);

    delay(2);

    msg.id = 0x351;
    msg.len = 8;
    if (storagemode == 0) {
      msg.buf[0] = lowByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
      msg.buf[1] = highByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
    } else {
      msg.buf[0] = lowByte(uint16_t((settings.StoreVsetpoint * settings.Scells) * 10));
      msg.buf[1] = highByte(uint16_t((settings.StoreVsetpoint * settings.Scells) * 10));
    }
    msg.buf[2] = lowByte(chargecurrent);
    msg.buf[3] = highByte(chargecurrent);
    msg.buf[4] = lowByte(discurrent);
    msg.buf[5] = highByte(discurrent);
    msg.buf[6] = 0x00;
    msg.buf[7] = 0x00;
    Can0.write(msg);

    delay(2);

    msg.id = 0x355;
    msg.len = 8;
    msg.buf[0] = lowByte(SOC);
    msg.buf[1] = highByte(SOC);
    msg.buf[2] = lowByte(SOH);   //static for now
    msg.buf[3] = highByte(SOH);  //static for now
    msg.buf[4] = 0x00;
    msg.buf[5] = 0x00;
    msg.buf[6] = 0x00;
    msg.buf[7] = 0x00;
    Can0.write(msg);

    delay(2);

    msg.id = 0x356;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(bms.getPackVoltage() * 100));
    msg.buf[1] = highByte(uint16_t(bms.getPackVoltage() * 100));
    msg.buf[2] = lowByte(long(currentact / 100));
    msg.buf[3] = highByte(long(currentact / 100));
    msg.buf[4] = lowByte(int16_t(bms.getAvgTemperature() * 10));
    msg.buf[5] = highByte(int16_t(bms.getAvgTemperature() * 10));
    msg.buf[6] = 0;
    msg.buf[7] = 0;
    Can0.write(msg);


    delay(2);

    msg.id = 0x35C;
    msg.len = 2;
    msg.buf[0] = 0xC0;  //fixed charge and discharge enable for verifcation
    msg.buf[1] = 0x00;
    Can0.write(msg);

    delay(2);

    msg.id = 0x35E;
    msg.len = 2;
#ifdef USING_TEENSY4
    msg.buf[0] = 'T';  //No idea how the naming works
    msg.buf[1] = 'P';  //No idea how the naming works
#else
    msg.buf[0] = "T";  //No idea how the naming works
    msg.buf[1] = "P";  //No idea how the naming works
#endif
    Can0.write(msg);
  } else {
    msg.id = 0x351;
    msg.len = 8;
    if (storagemode == 0) {
      msg.buf[0] = lowByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
      msg.buf[1] = highByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
    } else {
      msg.buf[0] = lowByte(uint16_t((settings.StoreVsetpoint * settings.Scells) * 10));
      msg.buf[1] = highByte(uint16_t((settings.StoreVsetpoint * settings.Scells) * 10));
    }
    msg.buf[2] = lowByte(chargecurrent);
    msg.buf[3] = highByte(chargecurrent);
    msg.buf[4] = lowByte(discurrent);
    msg.buf[5] = highByte(discurrent);
    msg.buf[6] = lowByte(uint16_t((settings.DischVsetpoint * settings.Scells) * 10));
    msg.buf[7] = highByte(uint16_t((settings.DischVsetpoint * settings.Scells) * 10));
    Can0.write(msg);

    msg.id = 0x355;
    msg.len = 8;
    msg.buf[0] = lowByte(SOC);
    msg.buf[1] = highByte(SOC);
    msg.buf[2] = lowByte(SOH);
    msg.buf[3] = highByte(SOH);
    msg.buf[4] = lowByte(SOC * 10);
    msg.buf[5] = highByte(SOC * 10);
    msg.buf[6] = 0;
    msg.buf[7] = 0;
    Can0.write(msg);

    msg.id = 0x356;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(bms.getPackVoltage() * 100));
    msg.buf[1] = highByte(uint16_t(bms.getPackVoltage() * 100));
    msg.buf[2] = lowByte(long(currentact / 100));
    msg.buf[3] = highByte(long(currentact / 100));
    msg.buf[4] = lowByte(int16_t(bms.getAvgTemperature() * 10));
    msg.buf[5] = highByte(int16_t(bms.getAvgTemperature() * 10));
    msg.buf[6] = 0;
    msg.buf[7] = 0;
    Can0.write(msg);

    delay(2);
    msg.id = 0x35A;
    msg.len = 8;
    msg.buf[0] = alarm[0];    //High temp  Low Voltage | High Voltage
    msg.buf[1] = alarm[1];    // High Discharge Current | Low Temperature
    msg.buf[2] = alarm[2];    //Internal Failure | High Charge current
    msg.buf[3] = alarm[3];    // Cell Imbalance
    msg.buf[4] = warning[0];  //High temp  Low Voltage | High Voltage
    msg.buf[5] = warning[1];  // High Discharge Current | Low Temperature
    msg.buf[6] = warning[2];  //Internal Failure | High Charge current
    msg.buf[7] = warning[3];  // Cell Imbalance
    Can0.write(msg);

    msg.id = 0x35E;
    msg.len = 8;
    msg.buf[0] = bmsname[0];
    msg.buf[1] = bmsname[1];
    msg.buf[2] = bmsname[2];
    msg.buf[3] = bmsname[3];
    msg.buf[4] = bmsname[4];
    msg.buf[5] = bmsname[5];
    msg.buf[6] = bmsname[6];
    msg.buf[7] = bmsname[7];
    Can0.write(msg);

    delay(2);
    msg.id = 0x370;
    msg.len = 8;
    msg.buf[0] = bmsmanu[0];
    msg.buf[1] = bmsmanu[1];
    msg.buf[2] = bmsmanu[2];
    msg.buf[3] = bmsmanu[3];
    msg.buf[4] = bmsmanu[4];
    msg.buf[5] = bmsmanu[5];
    msg.buf[6] = bmsmanu[6];
    msg.buf[7] = bmsmanu[7];
    Can0.write(msg);

    delay(2);
    msg.id = 0x373;
    msg.len = 8;
    msg.buf[0] = lowByte(uint16_t(bms.getLowCellVolt() * 1000));
    msg.buf[1] = highByte(uint16_t(bms.getLowCellVolt() * 1000));
    msg.buf[2] = lowByte(uint16_t(bms.getHighCellVolt() * 1000));
    msg.buf[3] = highByte(uint16_t(bms.getHighCellVolt() * 1000));
    msg.buf[4] = lowByte(uint16_t(bms.getLowTemperature() + 273.15));
    msg.buf[5] = highByte(uint16_t(bms.getLowTemperature() + 273.15));
    msg.buf[6] = lowByte(uint16_t(bms.getHighTemperature() + 273.15));
    msg.buf[7] = highByte(uint16_t(bms.getHighTemperature() + 273.15));
    Can0.write(msg);

    delay(2);
    msg.id = 0x379;  //Installed capacity
    msg.len = 2;
    msg.buf[0] = lowByte(uint16_t(settings.Pstrings * settings.CAP));
    msg.buf[1] = highByte(uint16_t(settings.Pstrings * settings.CAP));
    /*
        delay(2);
      msg.id  = 0x378; //Installed capacity
      msg.len = 2;
      //energy in 100wh/unit
      msg.buf[0] =
      msg.buf[1] =
      msg.buf[2] =
      msg.buf[3] =
      //energy out 100wh/unit
      msg.buf[4] =
      msg.buf[5] =
      msg.buf[6] =
      msg.buf[7] =
    */
    delay(2);
    msg.id = 0x372;
    msg.len = 8;
    msg.buf[0] = lowByte(bms.getNumModules());
    msg.buf[1] = highByte(bms.getNumModules());
    msg.buf[2] = 0x00;
    msg.buf[3] = 0x00;
    msg.buf[4] = 0x00;
    msg.buf[5] = 0x00;
    msg.buf[6] = 0x00;
    msg.buf[7] = 0x00;
    Can0.write(msg);
  }
}

void BMVmessage()  //communication with the Victron Color Control System over VEdirect
{
  lasttime = millis();
  x = 0;
  VE.write(13);
  VE.write(10);
  VE.write(myStrings[0]);
  VE.write(9);
  VE.print(bms.getPackVoltage() * 1000, 0);
  VE.write(13);
  VE.write(10);
  VE.write(myStrings[2]);
  VE.write(9);
  VE.print(currentact);
  VE.write(13);
  VE.write(10);
  VE.write(myStrings[4]);
  VE.write(9);
  VE.print(ampsecond * 0.27777777777778, 0);  //consumed ah
  VE.write(13);
  VE.write(10);
  VE.write(myStrings[6]);
  VE.write(9);
  VE.print(SOC * 10);  //SOC
  x = 8;
  while (x < 20) {
    VE.write(13);
    VE.write(10);
    VE.write(myStrings[x]);
    x++;
    VE.write(9);
    VE.write(myStrings[x]);
    x++;
  }
  VE.write(13);
  VE.write(10);
  VE.write("Checksum");
  VE.write(9);
  VE.write(0x50);  //0x59
  delay(10);

  while (x < 44) {
    VE.write(13);
    VE.write(10);
    VE.write(myStrings[x]);
    x++;
    VE.write(9);
    VE.write(myStrings[x]);
    x++;
  }
  /*
    VE.write(13);
    VE.write(10);
    VE.write(myStrings[32]);
    VE.write(9);
    VE.print(bms.getLowVoltage()*1000,0);
    VE.write(13);
    VE.write(10);
    VE.write(myStrings[34]);
    VE.write(9);
    VE.print(bms.getHighVoltage()*1000,0);
    x=36;

    while(x < 43)
    {
     VE.write(13);
     VE.write(10);
     VE.write(myStrings[x]);
     x ++;
     VE.write(9);
     VE.write(myStrings[x]);
     x ++;
    }
  */
  VE.write(13);
  VE.write(10);
  VE.write("Checksum");
  VE.write(9);
  VE.write(231);
}

/**
* Clear the serial buffer to ignore the next values
*/
void clear_serial_buffer()
{
  while (SERIALCONSOLE.available())
  {
    SERIALCONSOLE.read();
  }
}

size_t csize = 128;
char g_serial_buffer[128];
int  g_serial_index = 0;

/**
** Add the data to a buffer until CR or LF is found
*/
bool read_new_line()
{
  size_t index = 0;
  int inputbyte;

  inputbyte = SERIALCONSOLE.read();
  if (inputbyte != -1)
  {
    g_serial_buffer[g_serial_index++] = inputbyte;
    g_serial_buffer[g_serial_index] = '\0';
  }
  if (index >= csize - 1)
  {
    index = 0;
  }
  return (inputbyte == 10 || inputbyte == 13);
}

int getint(char *pbuffer, int size)
{
  if (size)
  {
    return atoi(pbuffer); 
  }
  return 0;
}

#define PARSEINT getint(input_line, input_size - 1)
//#define PARSEINT SERIALCONSOLE.parseInt()

// Settings menu
void menu() {
  char input_line[128];
  int  input_size;

  //incomingByte = SERIALCONSOLE.read();  // read the incoming byte:
  bool bnewline = read_new_line();
  if (!bnewline)
  {
    return ;
  }
  incomingByte = g_serial_buffer[0];
  input_size = g_serial_index;
  g_serial_index = 0;
  memcpy(input_line, &g_serial_buffer[1], input_size);

  //SERIALCONSOLE.print("Recieved: ");
  //SERIALCONSOLE.println(input_line);

  if (menuload == d_Debug_Settings) {
    switch (incomingByte) {

      case '1':
        menuload = s_main_menu;
        candebug = !candebug;
        incomingByte = 'd';
        break;

      case '2':
        menuload = s_main_menu;
        debugCur = !debugCur;
        incomingByte = 'd';
        break;

      case '3':
        menuload = s_main_menu;
        outputcheck = !outputcheck;
        if (outputcheck == 0) {
          contctrl = 0;
          digitalWrite(OUT1, LOW);
          digitalWrite(OUT2, LOW);
          digitalWrite(OUT3, LOW);
          digitalWrite(OUT4, LOW);
        }
        incomingByte = 'd';
        break;

      case '4':
        menuload = s_main_menu;
        inputcheck = !inputcheck;
        incomingByte = 'd';
        break;

      case '5':
        menuload = s_main_menu;
        settings.ESSmode = !settings.ESSmode;
        incomingByte = 'd';
        break;

      case '6':
        menuload = s_main_menu;
        cellspresent = bms.seriescells();
        incomingByte = 'd';
        break;

      case '7':
        menuload = s_main_menu;
        gaugedebug = !gaugedebug;
        incomingByte = 'd';
        break;

      case '8':
        menuload = s_main_menu;
        CSVdebug = !CSVdebug;
        incomingByte = 'd';
        break;

      case '9':
        menuload = s_main_menu;
        if (input_size > 0) {
          debugdigits = PARSEINT;
        }
        if (debugdigits > 4) {
          debugdigits = 2;
        }
        incomingByte = 'd';
        break;


      case 'b':
        menuload = s_main_menu;
        balon = !balon;
        incomingByte = 'd';
        break;

      case 'q':  //q for quit menu

        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;

      default:
        // if nothing else matches, do the default
        // default is optional
        break;
    }
  }

  if (menuload == c_Current_Sensor_Calibration) {
    switch (incomingByte) {


      case 99:  //c for calibrate zero offset

        calcur();
        break;

      case '1':
        menuload = s_main_menu;
        settings.invertcur = !settings.invertcur;
        incomingByte = 'c';
        break;

      case '2':
        menuload = s_main_menu;
        settings.voltsoc = !settings.voltsoc;
        incomingByte = 'c';
        break;

      case '3':
        menuload = s_main_menu;
        if (input_size > 0) {
          settings.ncur = PARSEINT;
        }
        menuload = s_main_menu;
        incomingByte = 'c';
        break;

      case '4':
        menuload = s_main_menu;
        if (input_size > 0) {
          settings.convlow = PARSEINT;
        }
        incomingByte = 'c';
        break;

      case '5':
        menuload = s_main_menu;
        if (input_size > 0) {
          settings.convhigh = PARSEINT;
        }
        incomingByte = 'c';
        break;

      case '6':
        menuload = s_main_menu;
        if (input_size > 0) {
          settings.CurDead = PARSEINT;
        }
        incomingByte = 'c';
        break;

      case '8':
        menuload = s_main_menu;
        if (input_size > 0) {
          settings.changecur = PARSEINT;
        }
        menuload = s_main_menu;
        incomingByte = 'c';
        break;

      case 'q':  //q for quite menu

        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;

      case 115:  //s for switch sensor
        settings.cursens++;
        if (settings.cursens > 3) {
          settings.cursens = 0;
        }
        /*
          if (settings.cursens == Analoguedual)
          {
            settings.cursens = Canbus;
            SERIALCONSOLE.println("  ");
            SERIALCONSOLE.print(" CANbus Current Sensor ");
            SERIALCONSOLE.println("  ");
          }
          else
          {
            settings.cursens = Analoguedual;
            SERIALCONSOLE.println("  ");
            SERIALCONSOLE.print(" Analogue Current Sensor ");
            SERIALCONSOLE.println("  ");
          }
        */
        menuload = s_main_menu;
        incomingByte = 'c';
        break;

      case '7':  //s for switch sensor
        settings.curcan++;
        if (settings.curcan > CurCanMax) {
          settings.curcan = 1;
        }
        menuload = s_main_menu;
        incomingByte = 'c';
        break;

      default:
        // if nothing else matches, do the default
        // default is optional
        break;
    }
  }

  if (menuload == i_Ignore_Value_Settings) {
    switch (incomingByte) {
      case '1':  //e dispaly settings
        if (SERIALCONSOLE.available() > 0) {
          settings.IgnoreTemp = PARSEINT;
        }
        if (settings.IgnoreTemp > 3) {
          settings.IgnoreTemp = 0;
        }
        bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
        menuload = s_main_menu;
        incomingByte = 'i';
        break;

      case '2':
        if (input_size > 0) {
          settings.IgnoreVolt = PARSEINT;
          settings.IgnoreVolt = settings.IgnoreVolt * 0.001;
          bms.setSensors(settings.IgnoreTemp, settings.IgnoreVolt, settings.DeltaVolt);
          // SERIALCONSOLE.println(settings.IgnoreVolt);
          menuload = s_main_menu;
          incomingByte = 'i';
        }
        break;

      case 'q':  //q to go back to main menu

        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;
    }
  }

  if (menuload == a_Alarm_and_Warning_Settings) {
    switch (incomingByte) {
      case '1':
        if (input_size > 0) {
          settings.WarnOff = PARSEINT;
          settings.WarnOff = settings.WarnOff * 0.001;
          menuload = s_main_menu;
          incomingByte = 'a';
        }
        break;

      case '2':
        if (input_size > 0) {
          settings.CellGap = PARSEINT;
          settings.CellGap = settings.CellGap * 0.001;
          menuload = s_main_menu;
          incomingByte = 'a';
        }
        break;

      case '3':
        if (input_size > 0) {
          settings.WarnToff = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'a';
        }
        break;

      case '4':
        if (input_size > 0) {
          settings.triptime = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'a';
        }
        break;

      case 'q':  //q to go back to main menu
        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;
    }
  }

  if (menuload == e_Charging_Settings)  //Charging settings
  {
    switch (incomingByte) {

      case 'q':  //q to go back to main menu

        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;

      case '1':
        if (input_size > 0) {
          settings.ChargeVsetpoint = PARSEINT;
          settings.ChargeVsetpoint = settings.ChargeVsetpoint / 1000;
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;

      case '2':
        if (input_size > 0) {
          settings.ChargeHys = PARSEINT;
          settings.ChargeHys = settings.ChargeHys / 1000;
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;

      case '4':
        if (input_size > 0) {
          settings.chargecurrentend = PARSEINT * 10;
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;

      case '3':
        if (input_size > 0) {
          settings.chargecurrentmax = PARSEINT * 10;
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;

      case '5':  //1 Over Voltage Setpoint
        settings.chargertype = settings.chargertype + 1;
        if (settings.chargertype > 8) {
          settings.chargertype = 0;
        }
        menuload = s_main_menu;
        incomingByte = 'e';
        break;

      case '6':
        if (input_size > 0) {
          settings.chargerspd = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;

      case '7':
        if (settings.ChargerDirect == 1) {
          settings.ChargerDirect = 0;
        } else {
          settings.ChargerDirect = 1;
        }
        menuload = s_main_menu;
        incomingByte = 'e';
        break;

      case '9':
        if (input_size > 0) {
          settings.ChargeTSetpoint = PARSEINT;
          if (settings.ChargeTSetpoint < settings.UnderTSetpoint) {
            settings.ChargeTSetpoint = settings.UnderTSetpoint;
          }
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;
      case '0':
        if (input_size > 0) {
          settings.chargecurrentcold = PARSEINT * 10;
          if (settings.chargecurrentcold > settings.chargecurrentmax) {
            settings.chargecurrentcold = settings.chargecurrentmax;
          }
          menuload = s_main_menu;
          incomingByte = 'e';
        }
        break;
    }
  }
  if (menuload == k_Contactor_and_Gauge_Settings) {
    switch (incomingByte) {
      case '1':
        if (input_size > 0) {
          settings.Pretime = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'k';
        }
        break;

      case '2':
        if (input_size > 0) {
          settings.Precurrent = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'k';
        }
        break;

      case '3':
        if (input_size > 0) {
          settings.conthold = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'k';
        }
        break;

      case '4':
        if (input_size > 0) {
          settings.gaugelow = PARSEINT;
          gaugedebug = 2;
          gaugeupdate();
          menuload = s_main_menu;
          incomingByte = 'k';
        }
        break;

      case '5':
        if (input_size > 0) {
          settings.gaugehigh = PARSEINT;
          gaugedebug = 3;
          gaugeupdate();
          menuload = s_main_menu;
          incomingByte = 'k';
        }
        break;

      case '6':
        settings.tripcont = !settings.tripcont;
        if (settings.tripcont > 1) {
          settings.tripcont = 0;
        }
        menuload = s_main_menu;
        incomingByte = 'k';
        break;

      case 'q':  //q to go back to main menu
        gaugedebug = 0;
        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;
    }
  }

  if (menuload == b_Battery_Settings) {
    switch (incomingByte) {
      case 'q':  //q to go back to main menu
        menuload = q_exit_menu;
        incomingByte = MAIN_MENU_KEY;
        break;

      case 'f':  //f factory settings
        loadSettings();
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.println(" Coded Settings Loaded ");
        SERIALCONSOLE.println("  ");
        menuload = s_main_menu;
        incomingByte = 'b';
        break;

      case 'r':  //r for reset
        SOCreset = 1;
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print(" mAh Reset ");
        SERIALCONSOLE.println("  ");
        menuload = s_main_menu;
        incomingByte = 'b';
        break;

      case '1':  //1 Over Voltage Setpoint
        if (input_size > 0) {
          settings.OverVSetpoint = PARSEINT;
          settings.OverVSetpoint = settings.OverVSetpoint / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'g':
        if (input_size > 0) {
          settings.StoreVsetpoint = PARSEINT;
          settings.StoreVsetpoint = settings.StoreVsetpoint / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }

      case 'h':
        if (input_size > 0) {
          settings.DisTaper = PARSEINT;
          settings.DisTaper = settings.DisTaper / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }

      case 'b':
        if (input_size > 0) {
          settings.socvolt[0] = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'c':
        if (input_size > 0) {
          settings.socvolt[1] = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'd':
        if (input_size > 0) {
          settings.socvolt[2] = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'e':
        if (input_size > 0) {
          settings.socvolt[3] = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'k':  //Discharge Voltage hysteresis
        if (input_size > 0) {
          settings.DischHys = PARSEINT;
          settings.DischHys = settings.DischHys / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case 'j':
        if (input_size > 0) {
          settings.DisTSetpoint = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '9':  //Discharge Voltage Setpoint
        if (input_size > 0) {
          settings.DischVsetpoint = PARSEINT;
          settings.DischVsetpoint = settings.DischVsetpoint / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '0':  //c Pstrings
        if (input_size > 0) {
          settings.Pstrings = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
          bms.setPstrings(settings.Pstrings);
        }
        else
        {
          SERIALCONSOLE.println(" No value available ");
        }
        break;

      case 'a':  //
        if (input_size > 0) {
          settings.Scells = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '2':  //2 Under Voltage Setpoint
        if (input_size > 0) {
          settings.UnderVSetpoint = PARSEINT;
          settings.UnderVSetpoint = settings.UnderVSetpoint / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '3':  //3 Over Temperature Setpoint
        if (input_size > 0) {
          settings.OverTSetpoint = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '4':  //4 Udner Temperature Setpoint
        if (input_size > 0) {
          settings.UnderTSetpoint = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '5':  //5 Balance Voltage Setpoint
        if (input_size > 0) {
          settings.balanceVoltage = PARSEINT;
          settings.balanceVoltage = settings.balanceVoltage / 1000;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '6':  //6 Balance Voltage Hystersis
        if (input_size > 0) {
          settings.balanceHyst = PARSEINT;
          settings.balanceHyst = settings.balanceHyst / 1000;
          menuload = s_main_menu;
          bms.setBalanceHyst(settings.balanceHyst);
          incomingByte = 'b';
        }
        break;

      case '7':  //7 Battery Capacity inAh
        if (input_size > 0) {
          settings.CAP = PARSEINT;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;

      case '8':  // discurrent in A
        if (input_size > 0) {
          settings.discurrentmax = PARSEINT * 10;
          menuload = s_main_menu;
          incomingByte = 'b';
        }
        break;
    }
  }

  if (menuload == s_main_menu) {
    switch (incomingByte) {
      case 'R':  //restart
        CPU_REBOOT;
        break;

      case 'i':  //Ignore Value Settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Ignore Value Settings");
        SERIALCONSOLE.print("1 - Temp Sensor Setting:");
        SERIALCONSOLE.println(settings.IgnoreTemp);
        SERIALCONSOLE.print("2 - Voltage Under Which To Ignore Cells:");
        SERIALCONSOLE.print(settings.IgnoreVolt * 1000, 0);
        SERIALCONSOLE.println("mV");
        SERIALCONSOLE.println("q - Go back to menu");
        menuload = i_Ignore_Value_Settings;
        break;

      case 'e':  //Charging settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Charging Settings");
        SERIALCONSOLE.print("1 - Cell Charge Voltage Limit Setpoint: ");
        SERIALCONSOLE.print(settings.ChargeVsetpoint * 1000, 0);
        SERIALCONSOLE.println("mV");
        SERIALCONSOLE.print("2 - Charge Hystersis: ");
        SERIALCONSOLE.print(settings.ChargeHys * 1000, 0);
        SERIALCONSOLE.println("mV");
        if (settings.chargertype > 0) {
          SERIALCONSOLE.print("3 - Pack Max Charge Current: ");
          SERIALCONSOLE.print(settings.chargecurrentmax * 0.1);
          SERIALCONSOLE.println("A");
          SERIALCONSOLE.print("4- Pack End of Charge Current: ");
          SERIALCONSOLE.print(settings.chargecurrentend * 0.1);
          SERIALCONSOLE.println("A");
        }
        SERIALCONSOLE.print("5- Charger Type: ");
        switch (settings.chargertype) {
          case 0:
            SERIALCONSOLE.print("Relay Control");
            break;
          case 1:
            SERIALCONSOLE.print("Brusa NLG5xx");
            break;
          case 2:
            SERIALCONSOLE.print("Volt Charger");
            break;
          case 3:
            SERIALCONSOLE.print("Eltek Charger");
            break;
          case 4:
            SERIALCONSOLE.print("Elcon Charger");
            break;
          case 5:
            SERIALCONSOLE.print("Victron/SMA");
            break;
          case 6:
            SERIALCONSOLE.print("Coda");
            break;
          case 7:
            SERIALCONSOLE.print("Pylon - TESTING ONLY");
            break;
          case 8:
            SERIALCONSOLE.print("Outlander Charger");
            break;
        }
        SERIALCONSOLE.println();
        if (settings.chargertype > 0) {
          SERIALCONSOLE.print("6- Charger Can Msg Spd: ");
          SERIALCONSOLE.print(settings.chargerspd);
          SERIALCONSOLE.println("mS");
          SERIALCONSOLE.println();
        }
        /*
          SERIALCONSOLE.print("7- Can Speed:");
          SERIALCONSOLE.print(settings.canSpeed/1000);
          SERIALCONSOLE.println("kbps");
        */
        SERIALCONSOLE.print("7 - Charger HV Connection: ");
        switch (settings.ChargerDirect) {
          case 0:
            SERIALCONSOLE.print(" Behind Contactors");
            break;
          case 1:
            SERIALCONSOLE.print("Direct To Battery HV");
            break;
        }
        SERIALCONSOLE.println();

        SERIALCONSOLE.print("9 - Charge Current derate Low: ");
        SERIALCONSOLE.print(settings.ChargeTSetpoint);
        SERIALCONSOLE.println(" C");
        SERIALCONSOLE.print("0 - Pack Cold Charge Current: ");
        SERIALCONSOLE.print(settings.chargecurrentcold * 0.1);
        SERIALCONSOLE.println("A");

        SERIALCONSOLE.println("q - Go back to menu");
        menuload = e_Charging_Settings;
        break;

      case 'a':  //Alarm and Warning settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Alarm and Warning Settings Menu");
        SERIALCONSOLE.print("1 - Voltage Warning Offset: ");
        SERIALCONSOLE.print(settings.WarnOff * 1000, 0);
        SERIALCONSOLE.println("mV");
        SERIALCONSOLE.print("2 - Cell Voltage Difference Alarm: ");
        SERIALCONSOLE.print(settings.CellGap * 1000, 0);
        SERIALCONSOLE.println("mV");
        SERIALCONSOLE.print("3 - Temp Warning Offset: ");
        SERIALCONSOLE.print(settings.WarnToff);
        SERIALCONSOLE.println(" C");
        /*
          SERIALCONSOLE.print("4 - Temp Warning Offset: ");
          SERIALCONSOLE.print(settings.UnderDur);
          SERIALCONSOLE.println(" mS");
        */
        SERIALCONSOLE.print("4 - Over and Under Voltage Delay: ");
        SERIALCONSOLE.print(settings.triptime);
        SERIALCONSOLE.println(" mS");

        menuload = a_Alarm_and_Warning_Settings;
        break;

      case 'k':  //contactor settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Contactor and Gauge Settings Menu");
        SERIALCONSOLE.print("1 - PreCharge Timer: ");
        SERIALCONSOLE.print(settings.Pretime);
        SERIALCONSOLE.println("mS");
        SERIALCONSOLE.print("2 - PreCharge Finish Current: ");
        SERIALCONSOLE.print(settings.Precurrent);
        SERIALCONSOLE.println(" mA");
        SERIALCONSOLE.print("3 - PWM contactor Hold 0-255 :");
        SERIALCONSOLE.println(settings.conthold);
        SERIALCONSOLE.print("4 - PWM for Gauge Low 0-255 :");
        SERIALCONSOLE.println(settings.gaugelow);
        SERIALCONSOLE.print("5 - PWM for Gauge High 0-255 :");
        SERIALCONSOLE.println(settings.gaugehigh);

        if (settings.ESSmode == 1) {
          SERIALCONSOLE.print("6 - ESS Main Contactor or Trip :");
          if (settings.tripcont == 0) {
            SERIALCONSOLE.println("Trip Shunt");
          } else {
            SERIALCONSOLE.println("Main Contactor and Precharge");
          }
        }

        menuload = k_Contactor_and_Gauge_Settings;
        break;

      case 'q':                   //q to go back to main menu
        EEPROM.put(0, settings);  //save all change to eeprom
        menuload = q_exit_menu;
        debug = 1;
        break;
      case 'd':  //d for debug settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Debug Settings Menu");
        SERIALCONSOLE.println("Toggle on/off");
        SERIALCONSOLE.print("1 - Can Debug :");
        SERIALCONSOLE.println(candebug);
        SERIALCONSOLE.print("2 - Current Debug :");
        SERIALCONSOLE.println(debugCur);
        SERIALCONSOLE.print("3 - Output Check :");
        SERIALCONSOLE.println(outputcheck);
        SERIALCONSOLE.print("4 - Input Check :");
        SERIALCONSOLE.println(inputcheck);
        SERIALCONSOLE.print("5 - ESS mode :");
        SERIALCONSOLE.println(settings.ESSmode);
        SERIALCONSOLE.print("6 - Cells Present Reset :");
        SERIALCONSOLE.println(cellspresent);
        SERIALCONSOLE.print("7 - Gauge Debug :");
        SERIALCONSOLE.println(gaugedebug);
        SERIALCONSOLE.print("8 - CSV Output :");
        SERIALCONSOLE.println(CSVdebug);
        SERIALCONSOLE.print("9 - Decimal Places to Show :");
        SERIALCONSOLE.println(debugdigits);
        SERIALCONSOLE.println("q - Go back to menu");
        menuload = d_Debug_Settings;
        break;

      case 'c':  //c for calibrate zero offset
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Current Sensor Calibration Menu");
        SERIALCONSOLE.println("c - To calibrate sensor offset");
        SERIALCONSOLE.print("s - Current Sensor Type : ");
        switch (settings.cursens) {
          case Analoguedual:
            SERIALCONSOLE.println(" Analogue Dual Current Sensor ");
            break;
          case Analoguesing:
            SERIALCONSOLE.println(" Analogue Single Current Sensor ");
            break;
          case Canbus:
            SERIALCONSOLE.println(" Canbus Current Sensor ");
            break;
          default:
            SERIALCONSOLE.println("Undefined");
            break;
        }
        SERIALCONSOLE.print("1 - invert current :");
        SERIALCONSOLE.println(settings.invertcur);
        SERIALCONSOLE.print("2 - Pure Voltage based SOC :");
        SERIALCONSOLE.println(settings.voltsoc);
        SERIALCONSOLE.print("3 - Current Multiplication :");
        SERIALCONSOLE.println(settings.ncur);
        if (settings.cursens == Analoguesing || settings.cursens == Analoguedual) {
          SERIALCONSOLE.print("4 - Analogue Low Range Conv:");
          SERIALCONSOLE.print(settings.convlow * 0.1, 1);
          SERIALCONSOLE.println(" mV/A");
        }
        if (settings.cursens == Analoguedual) {
          SERIALCONSOLE.print("5 - Analogue High Range Conv:");
          SERIALCONSOLE.print(settings.convhigh * 0.1, 1);
          SERIALCONSOLE.println(" mV/A");
        }
        if (settings.cursens == Analoguesing || settings.cursens == Analoguedual) {
          SERIALCONSOLE.print("6 - Current Sensor Deadband:");
          SERIALCONSOLE.print(settings.CurDead);
          SERIALCONSOLE.println(" mV");
        }
        if (settings.cursens == Analoguedual) {

          SERIALCONSOLE.print("8 - Current Channel ChangeOver:");
          SERIALCONSOLE.print(settings.changecur * 0.001);
          SERIALCONSOLE.println(" A");
        }
        if (settings.cursens == Canbus) {
          SERIALCONSOLE.print("7 -Can Current Sensor :");
          if (settings.curcan == LemCAB300) {
            SERIALCONSOLE.println(" LEM CAB300/500 series ");
          } else if (settings.curcan == LemCAB500) {
            SERIALCONSOLE.println(" LEM CAB500 Special ");
          } else if (settings.curcan == IsaScale) {
            SERIALCONSOLE.println(" IsaScale IVT-S ");
          } else if (settings.curcan == VictronLynx) {
            SERIALCONSOLE.println(" Victron Lynx VE.CAN Shunt");
          }
        }
        SERIALCONSOLE.println("q - Go back to menu");
        menuload = c_Current_Sensor_Calibration;
        break;

      case 'b':  //b for battery settings
        clear_serial_buffer();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.println("Battery Settings Menu");
        SERIALCONSOLE.println("r - Reset AH counter");
        SERIALCONSOLE.println("f - Reset to Coded Settings");
        SERIALCONSOLE.println("q - Go back to menu");
        SERIALCONSOLE.println();
        SERIALCONSOLE.println();
        SERIALCONSOLE.print("1 - Cell Over Voltage Setpoint: ");
        SERIALCONSOLE.print(settings.OverVSetpoint * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("2 - Cell Under Voltage Setpoint: ");
        SERIALCONSOLE.print(settings.UnderVSetpoint * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("3 - Over Temperature Setpoint: ");
        SERIALCONSOLE.print(settings.OverTSetpoint);
        SERIALCONSOLE.print("C");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("4 - Under Temperature Setpoint: ");
        SERIALCONSOLE.print(settings.UnderTSetpoint);
        SERIALCONSOLE.print("C");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("5 - Cell Balance Voltage Setpoint: ");
        SERIALCONSOLE.print(settings.balanceVoltage * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("6 - Balance Voltage Hystersis: ");
        SERIALCONSOLE.print(settings.balanceHyst * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("7 - Ah Battery Capacity: ");
        SERIALCONSOLE.print(settings.CAP);
        SERIALCONSOLE.print("Ah");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("8 - Pack Max Discharge: ");
        SERIALCONSOLE.print(settings.discurrentmax * 0.1);
        SERIALCONSOLE.print("A");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("9 - Cell Discharge Voltage Limit Setpoint: ");
        SERIALCONSOLE.print(settings.DischVsetpoint * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("0 - Slave strings in parallel: ");
        SERIALCONSOLE.print(settings.Pstrings);
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("a - Cells in Series per String: ");
        SERIALCONSOLE.print(settings.Scells);
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("b - setpoint 1: ");
        SERIALCONSOLE.print(settings.socvolt[0]);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("c - SOC setpoint 1:");
        SERIALCONSOLE.print(settings.socvolt[1]);
        SERIALCONSOLE.print("%");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("d - setpoint 2: ");
        SERIALCONSOLE.print(settings.socvolt[2]);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("e - SOC setpoint 2: ");
        SERIALCONSOLE.print(settings.socvolt[3]);
        SERIALCONSOLE.print("%");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("g - Storage Setpoint: ");
        SERIALCONSOLE.print(settings.StoreVsetpoint * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("h - Discharge Current Taper Offset: ");
        SERIALCONSOLE.print(settings.DisTaper * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("j - Discharge Current Temperature Derate : ");
        SERIALCONSOLE.print(settings.DisTSetpoint);
        SERIALCONSOLE.print("C");
        SERIALCONSOLE.println("  ");
        SERIALCONSOLE.print("k - Cell Discharge Voltage Hysteresis: ");
        SERIALCONSOLE.print(settings.DischHys * 1000, 0);
        SERIALCONSOLE.print("mV");
        SERIALCONSOLE.println();
        menuload = b_Battery_Settings;
        break;

      default:
        // if nothing else matches, do the default
        // default is optional
        break;
    }
  }

  if ((incomingByte == 's') && (menuload == q_exit_menu)) {
    SERIALCONSOLE.println();
    SERIALCONSOLE.println("MENU");
    SERIALCONSOLE.println("Debugging Paused");
    SERIALCONSOLE.print("Firmware Version : ");
    SERIALCONSOLE.print(firmver);
    SERIALCONSOLE.println("_Teensy4.0_Port");
    SERIALCONSOLE.println("b - Battery Settings");
    SERIALCONSOLE.println("a - Alarm and Warning Settings");
    SERIALCONSOLE.println("e - Charging Settings");
    SERIALCONSOLE.println("c - Current Sensor Calibration");
    SERIALCONSOLE.println("k - Contactor and Gauge Settings");
    SERIALCONSOLE.println("i - Ignore Value Settings");
    SERIALCONSOLE.println("d - Debug Settings");
    SERIALCONSOLE.println("q - exit menu");
    debug = 0;
    menuload = s_main_menu;
  }
}

void canread() {
#ifdef USING_TEENSY4
  if (Can0.read(inMsg) == 0) {
    return;
  }
#else
  Can0.read(inMsg);
#endif
  // Read data: len = data length, buf = data byte(s)
  if (settings.cursens == Canbus) {
    if (settings.curcan == 1) {
      switch (inMsg.id) {
        case 0x3c1:
          CAB500();
          break;

        case 0x3c2:
          CAB300();
          break;

        default:
          break;
      }
    }
    if (settings.curcan == 2) {
      switch (inMsg.id) {
        case 0x3c1:
          CAB500();
          break;

        case 0x3c2:
          CAB500();
          break;

        default:
          break;
      }
    }
    if (settings.curcan == 3) {
      switch (inMsg.id) {
        case 0x521:  //
          CANmilliamps = (long)((inMsg.buf[2] << 24) | (inMsg.buf[3] << 16) | (inMsg.buf[4] << 8) | (inMsg.buf[5]));
          if (settings.cursens == Canbus) {
            RawCur = CANmilliamps;
            getcurrent();
          }
          break;
        case 0x522:  //
          voltage1 = (long)((inMsg.buf[2] << 24) | (inMsg.buf[3] << 16) | (inMsg.buf[4] << 8) | (inMsg.buf[5]));
          break;
        case 0x523:  //
          voltage2 = (long)((inMsg.buf[2] << 24) | (inMsg.buf[3] << 16) | (inMsg.buf[4] << 8) | (inMsg.buf[5]));
          break;
        default:
          break;
      }
    }
    if (settings.curcan == 4) {
      if (pgnFromCANId(inMsg.id) == 0x1F214 && inMsg.buf[0] == 0)  // Check PGN and only use the first packet of each sequence
      {
        handleVictronLynx();
      }
    }
  }


  if (inMsg.id < 0x300)  //do VW BMS magic if ids are ones identified to be modules
  {
    if (candebug == 1) {
      bms.decodecan(inMsg, 1);  //do VW BMS if ids are ones identified to be modules
    } else {
      bms.decodecan(inMsg, 0);  //do VW BMS if ids are ones identified to be modules
    }
  }

  if ((inMsg.id & 0x1FFFFFFF) < 0x1A555440 && (inMsg.id & 0x1FFFFFFF) > 0x1A555400)  // Determine if ID is Temperature CAN-ID
  {
    if (candebug == 1) {
      bms.decodetemp(inMsg, 1, 1);
    } else {
      bms.decodetemp(inMsg, 0, 1);
    }
  }

  if ((inMsg.id & 0x1FFFFFFF) < 0x1A5555FF && (inMsg.id & 0x1FFFFFFF) > 0x1A5555EF)  // Determine if ID is Temperature CAN-ID FOR MEB
  {
    if (candebug == 1) {
      bms.decodetemp(inMsg, 1, 2);
    } else {
      bms.decodetemp(inMsg, 0, 2);
    }
  }

  if (candebug == 1) {
    SERIALCONSOLE.print(millis());
    if ((inMsg.id & 0x80000000) == 0x80000000)  // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (inMsg.id & 0x1FFFFFFF), inMsg.len);
    else
      sprintf(msgString, ",0x%.3lX,false,%1d", inMsg.id, inMsg.len);

    SERIALCONSOLE.print(msgString);

    if ((inMsg.id & 0x40000000) == 0x40000000) {  // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      SERIALCONSOLE.print(msgString);
    } else {
      for (byte i = 0; i < inMsg.len; i++) {
        sprintf(msgString, ", 0x%.2X", inMsg.buf[i]);
        SERIALCONSOLE.print(msgString);
      }
    }

    SERIALCONSOLE.println();
  }
}

void CAB300() {
  for (int i = 0; i < 4; i++) {
    inbox = (inbox << 8) | inMsg.buf[i];
  }
  CANmilliamps = inbox;
  if (CANmilliamps > (long int)0x80000000) {
    CANmilliamps -= (long int)0x80000000;
  } else {
    CANmilliamps = (0x80000000 - CANmilliamps) * -1;
  }
  if (settings.cursens == Canbus) {
    RawCur = CANmilliamps;
    getcurrent();
  }
  if (candebug == 1) {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(CANmilliamps);
    SERIALCONSOLE.print("mA ");
  }
}

void CAB500() {
  inbox = 0;
  for (int i = 1; i < 4; i++) {
    inbox = (inbox << 8) | inMsg.buf[i];
  }
  CANmilliamps = inbox;
  if (candebug == 1) {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(CANmilliamps, HEX);
  }
  if (CANmilliamps > 0x800000) {
    CANmilliamps -= 0x800000;
  } else {
    CANmilliamps = (0x800000 - CANmilliamps) * -1;
  }
  if (settings.cursens == Canbus) {
    RawCur = CANmilliamps;
    getcurrent();
  }
  if (candebug == 1) {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(CANmilliamps);
    SERIALCONSOLE.print("mA ");
  }
}

void handleVictronLynx() {
  if (inMsg.buf[4] == 0xff && inMsg.buf[3] == 0xff) return;
  int16_t current = (int)inMsg.buf[4] << 8;  // in 0.1A increments
  current |= inMsg.buf[3];
  CANmilliamps = current * 100;
  if (settings.cursens == Canbus) {
    RawCur = CANmilliamps;
    getcurrent();
  }
  if (candebug == 1) {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(CANmilliamps);
    SERIALCONSOLE.print("mA ");
  }
}


void currentlimit() {
  if (bmsstatus == Error) {
    discurrent = 0;
    chargecurrent = 0;
  }
  /*
    settings.PulseCh = 600; //Peak Charge current in 0.1A
    settings.PulseChDur = 5000; //Ms of discharge pulse derating
    settings.PulseDi = 600; //Peak Charge current in 0.1A
    settings.PulseDiDur = 5000; //Ms of discharge pulse derating
  */
  else {

    ///Start at no derating///
    discurrent = settings.discurrentmax;
    chargecurrent = settings.chargecurrentmax;


    ///////All hard limits to into zeros
    if (bms.getLowTemperature() < settings.UnderTSetpoint) {
      //discurrent = 0; Request Daniel
      chargecurrent = settings.chargecurrentcold;
    }
    if (bms.getHighTemperature() > settings.OverTSetpoint) {
      discurrent = 0;
      chargecurrent = 0;
    }
    if (bms.getHighCellVolt() > settings.OverVSetpoint) {
      chargecurrent = 0;
    }
    if (bms.getHighCellVolt() > settings.OverVSetpoint) {
      chargecurrent = 0;
    }
    if (bms.getLowCellVolt() < settings.UnderVSetpoint || bms.getLowCellVolt() < settings.DischVsetpoint) {
      discurrent = 0;
    }


    //Modifying discharge current///

    if (discurrent > 0) {
      //Temperature based///

      if (bms.getHighTemperature() > settings.DisTSetpoint) {
        discurrent = discurrent - map(bms.getHighTemperature(), settings.DisTSetpoint, settings.OverTSetpoint, 0, settings.discurrentmax);
      }
      //Voltagee based///
      if (bms.getLowCellVolt() < (settings.DischVsetpoint + settings.DisTaper)) {
        discurrent = discurrent - map(bms.getLowCellVolt(), settings.DischVsetpoint, (settings.DischVsetpoint + settings.DisTaper), settings.discurrentmax, 0);
      }
    }

    //Modifying Charge current///
    if (chargecurrent > settings.chargecurrentcold) {
      //Temperature based///
      if (bms.getLowTemperature() < settings.ChargeTSetpoint) {
        chargecurrent = chargecurrent - map(bms.getLowTemperature(), settings.UnderTSetpoint, settings.ChargeTSetpoint, (settings.chargecurrentmax - settings.chargecurrentcold), 0);
      }
      //Voltagee based///
      if (storagemode == 1) {
        if (bms.getHighCellVolt() > (settings.StoreVsetpoint - settings.ChargeHys)) {
          chargecurrent = chargecurrent - map(bms.getHighCellVolt(), (settings.StoreVsetpoint - settings.ChargeHys), settings.StoreVsetpoint, settings.chargecurrentend, settings.chargecurrentmax);
        }
      } else {
        if (bms.getHighCellVolt() > (settings.ChargeVsetpoint - settings.ChargeHys)) {
          chargecurrent = chargecurrent - map(bms.getHighCellVolt(), (settings.ChargeVsetpoint - settings.ChargeHys), settings.ChargeVsetpoint, 0, (settings.chargecurrentmax - settings.chargecurrentend));
        }
      }
    }
  }
  ///No negative currents///

  if (discurrent < 0) {
    discurrent = 0;
  }
  if (chargecurrent < 0) {
    chargecurrent = 0;
  }
}



void inputdebug() {
  SERIALCONSOLE.println();
  SERIALCONSOLE.print("Input: ");
  if (digitalRead(IN1)) {
    SERIALCONSOLE.print("1 ON  ");
  } else {
    SERIALCONSOLE.print("1 OFF ");
  }
  if (digitalRead(IN3)) {
    SERIALCONSOLE.print("2 ON  ");
  } else {
    SERIALCONSOLE.print("2 OFF ");
  }
  if (digitalRead(IN3)) {
    SERIALCONSOLE.print("3 ON  ");
  } else {
    SERIALCONSOLE.print("3 OFF ");
  }
  if (digitalRead(IN4)) {
    SERIALCONSOLE.print("4 ON  ");
  } else {
    SERIALCONSOLE.print("4 OFF ");
  }
  SERIALCONSOLE.println();
}

void outputdebug() {
  if (outputstate < 5) {
    digitalWrite(OUT1, HIGH);
    digitalWrite(OUT2, HIGH);
    digitalWrite(OUT3, HIGH);
    digitalWrite(OUT4, HIGH);
    analogWrite(OUT5, 255);
    analogWrite(OUT6, 255);
    analogWrite(OUT7, 255);
    analogWrite(OUT8, 255);
    outputstate++;
  } else {
    digitalWrite(OUT1, LOW);
    digitalWrite(OUT2, LOW);
    digitalWrite(OUT3, LOW);
    digitalWrite(OUT4, LOW);
    analogWrite(OUT5, 0);
    analogWrite(OUT6, 0);
    analogWrite(OUT7, 0);
    analogWrite(OUT8, 0);
    outputstate++;
  }
  if (outputstate > 10) {
    outputstate = 0;
  }
}

void sendcommand() {
  msg.id = controlid;
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
  delay(1);
  msg.id = controlid;
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


void sendbalancingtest() {
  if (balinit == 0) {
    msg.id = 0x1A555418;
    msg.len = 8;
    msg.flags.extended = 1;
    msg.buf[0] = 0xFE;
    msg.buf[1] = 0xFE;
    msg.buf[2] = 0xFE;
    msg.buf[3] = 0xFE;
    msg.buf[4] = 0xFE;
    msg.buf[5] = 0xFE;
    msg.buf[6] = 0xFE;
    msg.buf[7] = 0xFE;
    Can0.write(msg);
    delay(1);

    msg.id = 0x1A555419;
    Can0.write(msg);
    delay(1);

    msg.id = 0x1A555416;
    Can0.write(msg);
    delay(1);

    msg.id = 0x1A555417;
    Can0.write(msg);
    delay(1);
    balinit = 1;
  }

  if (balcycle == 1) {
    if (balon == 1) {
      msg.id = 0x1A555418;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x08;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0x00;
      msg.buf[5] = 0x00;
      msg.buf[6] = 0x00;
      msg.buf[7] = 0x00;
      Can0.write(msg);
      delay(1);
      msg.id = 0x1A555419;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0xFE;
      msg.buf[5] = 0xFE;
      msg.buf[6] = 0xFE;
      msg.buf[7] = 0xFE;
      Can0.write(msg);
      delay(1);

      msg.id = 0x1A555416;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x08;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x08;
      msg.buf[4] = 0x00;
      msg.buf[5] = 0x08;
      msg.buf[6] = 0x00;
      msg.buf[7] = 0x08;
      Can0.write(msg);
      delay(1);
      msg.id = 0x1A555417;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x08;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x08;
      msg.buf[4] = 0xFE;
      msg.buf[5] = 0xFE;
      msg.buf[6] = 0xFE;
      msg.buf[7] = 0xFE;
      Can0.write(msg);
      delay(1);
    } else {
      msg.id = 0x1A555418;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0x00;
      msg.buf[5] = 0x00;
      msg.buf[6] = 0x00;
      msg.buf[7] = 0x00;
      Can0.write(msg);
      delay(1);
      msg.id = 0x1A555419;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0xFE;
      msg.buf[5] = 0xFE;
      msg.buf[6] = 0xFE;
      msg.buf[7] = 0xFE;
      Can0.write(msg);
      delay(1);

      msg.id = 0x1A555416;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0x00;
      msg.buf[5] = 0x00;
      msg.buf[6] = 0x00;
      msg.buf[7] = 0x00;
      Can0.write(msg);
      delay(1);
      msg.id = 0x1A555417;
      msg.len = 8;
      msg.flags.extended = 1;
      msg.buf[0] = 0x00;
      msg.buf[1] = 0x00;
      msg.buf[2] = 0x00;
      msg.buf[3] = 0x00;
      msg.buf[4] = 0xFE;
      msg.buf[5] = 0xFE;
      msg.buf[6] = 0xFE;
      msg.buf[7] = 0xFE;
      Can0.write(msg);
      delay(1);
    }
  }



  balcycle++;

  if (balcycle > 10) {
    balcycle = 0;
  }

  msg.flags.extended = 0;
}

void resetwdog() {
#ifdef USING_TEENSY4
  wdt.feed();
#else
  noInterrupts();      //   No - reset WDT
  WDOG_REFRESH = 0xA602;
  WDOG_REFRESH = 0xB480;
  interrupts();
#endif  //USING_TEENSY4
}

void pwmcomms() {
  int p = 0;
  p = map((currentact * 0.001), pwmcurmin, pwmcurmax, 50, 255);
  analogWrite(OUT7, p);
  /*
    SERIALCONSOLE.println();
      SERIALCONSOLE.print(p*100/255);
      SERIALCONSOLE.print(" OUT8 ");
  */
  if (bms.getLowCellVolt() < settings.UnderVSetpoint) {
    analogWrite(OUT8, 224);  //12V to 10V converter 1.5V
  } else {
    p = map(SOC, 0, 100, 220, 50);
    analogWrite(OUT8, p);  //2V to 10V converter 1.5-10V
  }
  /*
      SERIALCONSOLE.println();
      SERIALCONSOLE.print(p*100/255);
      SERIALCONSOLE.print(" OUT7 ");
  */
}

void dashupdate() {
  Serial2.write("stat.txt=");
  Serial2.write(0x22);
  if (settings.ESSmode == 1) {
    switch (bmsstatus) {
      case (Boot):
        Serial2.print(" Active ");
        break;
      case (Error):
        Serial2.print(" Error ");
        break;
    }
  } else {
    switch (bmsstatus) {
      case (Boot):
        Serial2.print(" Boot ");
        break;

      case (Ready):
        Serial2.print(" Ready ");
        break;

      case (Precharge):
        Serial2.print(" Precharge ");
        break;

      case (Drive):
        Serial2.print(" Drive ");
        break;

      case (Charge):
        Serial2.print(" Charge ");
        break;

      case (Error):
        Serial2.print(" Error ");
        break;
    }
  }
  Serial2.write(0x22);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("soc.val=");
  Serial2.print(SOC);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("soc1.val=");
  Serial2.print(SOC);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("current.val=");
  Serial2.print(currentact / 100, 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("temp.val=");
  Serial2.print(bms.getAvgTemperature(), 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("templow.val=");
  Serial2.print(bms.getLowTemperature(), 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("temphigh.val=");
  Serial2.print(bms.getHighTemperature(), 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("volt.val=");
  Serial2.print(bms.getPackVoltage() * 10, 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("lowcell.val=");
  Serial2.print(bms.getLowCellVolt() * 1000, 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("highcell.val=");
  Serial2.print(bms.getHighCellVolt() * 1000, 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("firm.val=");
  Serial2.print(firmver);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.print("celldelta.val=");
  Serial2.print((bms.getHighCellVolt() - bms.getLowCellVolt()) * 1000, 0);
  Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
  /*
    Serial2.print("cellbal.val=");
    Serial2.print(bms.getBalancing());
    Serial2.write(0xff);  // We always have to send this three lines after each command sent to the nextion display.
    Serial2.write(0xff);
    Serial2.write(0xff);
  */
}


void Serialexp() {
  /*
    incomingByte = SERIALBMS.read(); // read the incoming byte:
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(incomingByte);
    SERIALCONSOLE.print("|");

    incomingByte = SERIALBMS.read(); // read the incoming byte:
    if (incomingByte == 0xFF)
    {
    SERIALCONSOLE.println();
    SERIALCONSOLE.print(incomingByte);
    incomingByte = SERIALBMS.read(); // read the incoming byte:
    SERIALCONSOLE.print("|");
    SERIALCONSOLE.print(incomingByte);
    SERIALCONSOLE.print("|");
    SERIALCONSOLE.print(incomingByte);
    if (settings.Serialexp == 1) //Do Serial Master Things
    {
    SERIALCONSOLE.print(SERIALBMS.read(), HEX);
    SERIALCONSOLE.print("|");
    SERIALCONSOLE.print(SERIALBMS.read(), HEX);
    }
    if (settings.Serialexp == 2) //Do Serial Slave Things
    {
    switch (incomingByte)
    {
    case 0x00: //q to go back to main menu
    if (SerialID == 0)
    {
      SerialID = SERIALBMS.read();
      SERIALBMS.write(0x01); //response is 1 higher than sent id
      SERIALBMS.write(SerialID);

      SERIALCONSOLE.print("New ID : ");
      SERIALCONSOLE.print(SerialID);
    }
    else
    {
      SERIALBMS.write(0xFF);
      SERIALBMS.write(0x00);
      SERIALBMS.write(SERIALBMS.read());
    }
    break;
    }
    }
    }
  */
}

void SerialReqData() {
  /*
    SERIALBMS.write(0x12);
  */
}

void Serialslaveinit() {
  /*
    int buff[8];
    while (1 == 1)
    {
    for (int I = 1; I < 51; I++)
    {
      SERIALBMS.write(0xFF);
      SERIALBMS.write(0x00);
      SERIALBMS.write(I);
      SERIALCONSOLE.write(" | ");
      delay(2);
      if (SERIALBMS.available() > 0)
      {
        for (int x = 0; x < 4; x++)
        {
          buff[x] = SERIALBMS.read();
          SERIALCONSOLE.write(buff[0]);
          SERIALCONSOLE.write(buff[1]);
          SERIALCONSOLE.write(buff[2]);
        }
        if (buff[0] = 0xFF)
        {
          if (buff[1] == I)
          {
            break;
          }
        }
      }
      else
      {
        SERIALCONSOLE.write("No Serial Slaves Found");
        break;
      }

    }
    break;
    }
  */
}


void chargercomms() {
  if (settings.chargertype == Elcon) {
    msg.id = 0x1806E5F4;  //broadcast to all Elteks
    msg.len = 8;
    msg.flags.extended = 1;
    msg.buf[0] = highByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));
    msg.buf[1] = lowByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));
    msg.buf[2] = highByte(chargecurrent / ncharger);
    msg.buf[3] = lowByte(chargecurrent / ncharger);
    msg.buf[4] = 0x00;
    msg.buf[5] = 0x00;
    msg.buf[6] = 0x00;
    msg.buf[7] = 0x00;

    Can0.write(msg);
    msg.flags.extended = 0;
  }

  if (settings.chargertype == Eltek) {
    msg.id = 0x2FF;  //broadcast to all Elteks
    msg.len = 7;
    msg.buf[0] = 0x01;
    msg.buf[1] = lowByte(1000);
    msg.buf[2] = highByte(1000);
    msg.buf[3] = lowByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));
    msg.buf[4] = highByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));
    msg.buf[5] = lowByte(chargecurrent / ncharger);
    msg.buf[6] = highByte(chargecurrent / ncharger);

    Can0.write(msg);
  }
  if (settings.chargertype == BrusaNLG5) {
    msg.id = chargerid1;
    msg.len = 7;
    msg.buf[0] = 0x80;
    /*
      if (chargertoggle == 0)
      {
      msg.buf[0] = 0x80;
      chargertoggle++;
      }
      else
      {
      msg.buf[0] = 0xC0;
      chargertoggle = 0;
      }
    */
    if (digitalRead(IN2) == LOW)  //Gen OFF
    {
      msg.buf[1] = highByte(maxac1 * 10);
      msg.buf[2] = lowByte(maxac1 * 10);
    } else {
      msg.buf[1] = highByte(maxac2 * 10);
      msg.buf[2] = lowByte(maxac2 * 10);
    }
    msg.buf[5] = highByte(chargecurrent / ncharger);
    msg.buf[6] = lowByte(chargecurrent / ncharger);
    msg.buf[3] = highByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) - chargerendbulk) * 10));
    msg.buf[4] = lowByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) - chargerendbulk) * 10));
    Can0.write(msg);

    delay(2);

    msg.id = chargerid2;
    msg.len = 7;
    msg.buf[0] = 0x80;
    if (digitalRead(IN2) == LOW)  //Gen OFF
    {
      msg.buf[1] = highByte(maxac1 * 10);
      msg.buf[2] = lowByte(maxac1 * 10);
    } else {
      msg.buf[1] = highByte(maxac2 * 10);
      msg.buf[2] = lowByte(maxac2 * 10);
    }
    msg.buf[3] = highByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) - chargerend) * 10));
    msg.buf[4] = lowByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) - chargerend) * 10));
    msg.buf[5] = highByte(chargecurrent / ncharger);
    msg.buf[6] = lowByte(chargecurrent / ncharger);
    Can0.write(msg);
  }
  if (settings.chargertype == ChevyVolt) {
    msg.id = 0x30E;
    msg.len = 1;
    msg.buf[0] = 0x02;  //only HV charging , 0x03 hv and 12V charging
    Can0.write(msg);

    msg.id = 0x304;
    msg.len = 4;
    msg.buf[0] = 0x40;  //fixed
    if ((chargecurrent * 2) > 255) {
      msg.buf[1] = 255;
    } else {
      msg.buf[1] = (chargecurrent * 2);
    }
    if ((settings.ChargeVsetpoint * settings.Scells) > 200) {
      msg.buf[2] = highByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 2));
      msg.buf[3] = lowByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 2));
    } else {
      msg.buf[2] = highByte(400);
      msg.buf[3] = lowByte(400);
    }
    Can0.write(msg);
  }

  if (settings.chargertype == Coda) {
    msg.id = 0x050;
    msg.len = 8;
    msg.buf[0] = 0x00;
    msg.buf[1] = 0xDC;
    if ((settings.ChargeVsetpoint * settings.Scells) > 200) {
      msg.buf[2] = highByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
      msg.buf[3] = lowByte(uint16_t((settings.ChargeVsetpoint * settings.Scells) * 10));
    } else {
      msg.buf[2] = highByte(400);
      msg.buf[3] = lowByte(400);
    }
    msg.buf[4] = 0x00;
    if ((settings.ChargeVsetpoint * settings.Scells) * chargecurrent < 3300) {
      msg.buf[5] = highByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) * chargecurrent) / 240));
      msg.buf[6] = highByte(uint16_t(((settings.ChargeVsetpoint * settings.Scells) * chargecurrent) / 240));
    } else  //15 A AC limit
    {
      msg.buf[5] = 0x00;
      msg.buf[6] = 0x96;
    }
    msg.buf[7] = 0x01;  //HV charging
    Can0.write(msg);
  }

  if (settings.chargertype == Outlander) {

    msg.id = 0x285;
    msg.len = 8;
    msg.buf[0] = 0x0;
    msg.buf[1] = 0x0;
    msg.buf[2] = 0xb6;
    msg.buf[3] = 0x0;
    msg.buf[4] = 0x0;
    msg.buf[5] = 0x0;
    msg.buf[6] = 0x0;
    Can0.write(msg);
    delay(2);

    msg.id = 0x286;
    msg.len = 8;
    msg.buf[0] = highByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));  //volage
    msg.buf[1] = lowByte(uint16_t(settings.ChargeVsetpoint * settings.Scells * 10));
    if (chargecurrent / ncharger > 120) {
      msg.buf[2] = 120;
    } else {
      msg.buf[2] = (chargecurrent / ncharger);
    }
    msg.buf[3] = 0x0;
    msg.buf[4] = 0x0;
    msg.buf[5] = 0x0;
    msg.buf[6] = 0x0;
    Can0.write(msg);
  }
}

int pgnFromCANId(int canId) {
  if ((canId & 0x10000000) == 0x10000000) {
    return (canId & 0x03FFFF00) >> 8;
  } else {
    return canId;  // not sure if this is really right?
  }
}

void isrCP() {
  if (digitalRead(IN4) == LOW) {
    duration = micros() - pilottimer;
    pilottimer = micros();
  } else {
    accurlim = ((duration - (micros() - pilottimer + 35)) * 60) / duration;  //pilottimer + "xx" optocoupler decade ms
  }
}  // ******** end of isr CP ********

// TEESNY4 has no low voltage detector
#ifndef USING_TEENSY4
void low_voltage_isr(void) {
  EEPROM.update(1000, uint8_t(SOC));

  PMC_LVDSC2 |= PMC_LVDSC2_LVWACK;  // clear if we can
  PMC_LVDSC1 |= PMC_LVDSC1_LVDACK;

  SERIALCONSOLE.println();
  SERIALCONSOLE.println("GoodBye");
}
#endif
////////END///////////
