#include <HIDPowerDevice.h>

// String constants
const char STRING_DEVICECHEMISTRY[] PROGMEM = "LiP";
const byte bDeviceChemistry = IDEVICECHEMISTRY;

const char STRING_OEMVENDOR[] PROGMEM = "BatteryVendor";
const byte bOEMVendor = IOEMVENDOR;

const char STRING_SERIAL[] PROGMEM = "1234";

PresentStatus iPresentStatus = {};

byte bRechargable = 1;
byte bCapacityMode = 0;  // unit: 0=mAh, 1=mWh, 2=%

// Physical parameters
const uint16_t iConfigVoltage = 1509; // centiVolt
uint16_t iVoltage =1499; // centiVolt
uint16_t iRunTimeToEmpty = 0;
uint16_t iAvgTimeToFull = 7200;
uint16_t iAvgTimeToEmpty = 7200;
uint16_t iRemainTimeLimit = 600;
int16_t  iDelayBe4Reboot = -1;
int16_t  iDelayBe4ShutDown = -1;
uint16_t iManufacturerDate = 0; // initialized in setup function
byte iAudibleAlarmCtrl = 2; // 1 - Disabled, 2 - Enabled, 3 - Muted


// Parameters for ACPI compliancy
const uint16_t iDesignCapacity = 58003*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)
byte iWarnCapacityLimit = 10; // warning at 10%
byte iRemnCapacityLimit = 5; // low at 5%
const byte bCapacityGranularity1 = 1;
const byte bCapacityGranularity2 = 1;
uint16_t iFullChargeCapacity = 40690*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)

uint16_t iRemaining[BATTERY_COUNT] = {}; // remaining charge
uint16_t iPrevRemaining=0;
bool bCharging = false;

void setup() {
#ifdef CDC_ENABLED
  Serial.begin(57600);
#endif

  for (int i = 0; i < BATTERY_COUNT; i++) {
    // initialize each battery with 50% charge
    iRemaining[i] = 0.50f*iFullChargeCapacity;

#ifdef CDC_ENABLED
    // Used for debugging purposes. 
    PowerDevice[i].setOutput(Serial);
#endif

    PowerDevice[i].SetFeature(0xFF00 | PowerDevice[i].bSerial, STRING_SERIAL, strlen_P(STRING_SERIAL));
  }

  pinMode(LED_BUILTIN, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running.

  for (int i = 0; i < BATTERY_COUNT; i++) {
    PowerDevice[i].SetFeature(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    PowerDevice[i].SetFeature(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));
    PowerDevice[i].SetFeature(HID_PD_AVERAGETIME2FULL, &iAvgTimeToFull, sizeof(iAvgTimeToFull));
    PowerDevice[i].SetFeature(HID_PD_AVERAGETIME2EMPTY, &iAvgTimeToEmpty, sizeof(iAvgTimeToEmpty));
    PowerDevice[i].SetFeature(HID_PD_REMAINTIMELIMIT, &iRemainTimeLimit, sizeof(iRemainTimeLimit));
    PowerDevice[i].SetFeature(HID_PD_DELAYBE4REBOOT, &iDelayBe4Reboot, sizeof(iDelayBe4Reboot));
    PowerDevice[i].SetFeature(HID_PD_DELAYBE4SHUTDOWN, &iDelayBe4ShutDown, sizeof(iDelayBe4ShutDown));

    PowerDevice[i].SetFeature(HID_PD_RECHARGEABLE, &bRechargable, sizeof(bRechargable));
    PowerDevice[i].SetFeature(HID_PD_CAPACITYMODE, &bCapacityMode, sizeof(bCapacityMode));
    PowerDevice[i].SetFeature(HID_PD_CONFIGVOLTAGE, &iConfigVoltage, sizeof(iConfigVoltage));
    PowerDevice[i].SetFeature(HID_PD_VOLTAGE, &iVoltage, sizeof(iVoltage));

    PowerDevice[i].setStringFeature(HID_PD_IDEVICECHEMISTRY, &bDeviceChemistry, STRING_DEVICECHEMISTRY);
    PowerDevice[i].setStringFeature(HID_PD_IOEMINFORMATION, &bOEMVendor, STRING_OEMVENDOR);
    PowerDevice[i].SetFeature(0xFF00 | PowerDevice[i].bManufacturer, STRING_OEMVENDOR, strlen_P(STRING_OEMVENDOR));

    PowerDevice[i].SetFeature(HID_PD_AUDIBLEALARMCTRL, &iAudibleAlarmCtrl, sizeof(iAudibleAlarmCtrl));

    PowerDevice[i].SetFeature(HID_PD_DESIGNCAPACITY, &iDesignCapacity, sizeof(iDesignCapacity));
    PowerDevice[i].SetFeature(HID_PD_FULLCHRGECAPACITY, &iFullChargeCapacity, sizeof(iFullChargeCapacity));
    PowerDevice[i].SetFeature(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));
    PowerDevice[i].SetFeature(HID_PD_WARNCAPACITYLIMIT, &iWarnCapacityLimit, sizeof(iWarnCapacityLimit));
    PowerDevice[i].SetFeature(HID_PD_REMNCAPACITYLIMIT, &iRemnCapacityLimit, sizeof(iRemnCapacityLimit));
    PowerDevice[i].SetFeature(HID_PD_CPCTYGRANULARITY1, &bCapacityGranularity1, sizeof(bCapacityGranularity1));
    PowerDevice[i].SetFeature(HID_PD_CPCTYGRANULARITY2, &bCapacityGranularity2, sizeof(bCapacityGranularity2));

    uint16_t year = 2024, month = 10, day = 12;
    iManufacturerDate = (year - 1980)*512 + month*32 + day; // from 4.2.6 Battery Settings in "Universal Serial Bus Usage Tables for HID Power Devices"
    PowerDevice[i].SetFeature(HID_PD_MANUFACTUREDATE, &iManufacturerDate, sizeof(iManufacturerDate));
  }
}

void loop() {
  //*********** Measurements Unit ****************************
  int iBattSoc = analogRead(PIN_A7); // potensiometer value in [0,1024)

  for (int i = BATTERY_COUNT-1; i > 0; i--)
    iRemaining[i] = iRemaining[i-1]; // propagate charge level from first to last battery
  iRemaining[0] = (uint16_t)(round((float)iFullChargeCapacity*iBattSoc/1024));

  iRunTimeToEmpty = (uint16_t)round((float)iAvgTimeToEmpty*iRemaining[0]/iFullChargeCapacity);

  if (iRemaining[0] > iPrevRemaining + 1) // add a bit hysteresis
    bCharging = true;
  else if (iRemaining[0] < iPrevRemaining - 1) // add a bit hysteresis
    bCharging = false;

  // Charging
  iPresentStatus.Charging = bCharging;
  iPresentStatus.ACPresent = bCharging; // assume charging implies AC present
  iPresentStatus.FullyCharged = (iRemaining[0] == iFullChargeCapacity);

  // Discharging
  if(!bCharging) { // assume not charging implies discharging
    iPresentStatus.Discharging = 1;
    // if(iRemaining[0] < iRemnCapacityLimit) iPresentStatus.BelowRemainingCapacityLimit = 1;

    iPresentStatus.RemainingTimeLimitExpired = (iRunTimeToEmpty < iRemainTimeLimit);
  } else {
    iPresentStatus.Discharging = 0;
    iPresentStatus.RemainingTimeLimitExpired = 0;
  }

  // Shutdown requested
  if(iDelayBe4ShutDown > 0 ) {
      iPresentStatus.ShutdownRequested = 1;
#ifdef CDC_ENABLED
      Serial.println("shutdown requested");
#endif
  } else {
    iPresentStatus.ShutdownRequested = 0;
  }

  // Shutdown imminent
  if((iPresentStatus.ShutdownRequested) || (iPresentStatus.RemainingTimeLimitExpired)) {
    iPresentStatus.ShutdownImminent = 1;
#ifdef CDC_ENABLED
    Serial.println("shutdown imminent");
#endif
  } else {
    iPresentStatus.ShutdownImminent = 0;
  }

  iPresentStatus.BatteryPresent = 1;


  //************ Delay ****************************************
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off;

  //************ Bulk send or interrupt ***********************
  int res = 0;
  for (int i = 0; i < BATTERY_COUNT; i++) {
    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));

    if((res >= 0) && !bCharging)
      res = PowerDevice[i].SendReport(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));

    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));
  }

  iPrevRemaining = iRemaining[0];

#ifdef CDC_ENABLED
  Serial.println(iRemaining[0]);
  Serial.println(iRunTimeToEmpty);

  Serial.print("SendReport res=");
  Serial.println(res);
#endif
}
