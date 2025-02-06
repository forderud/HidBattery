#include <HIDPowerDevice.h>
//#define ENABLE_POTENTIOMETER // uncomment to enable potentiometer

// String constants
const char STRING_DEVICECHEMISTRY[] PROGMEM = "LiP";
const byte bDeviceChemistry = IDEVICECHEMISTRY;

const char STRING_OEMVENDOR[] PROGMEM = "BatteryVendor";

const char STRING_SERIAL[] PROGMEM = "1234";

PresentStatus iPresentStatus = {};

byte bCapacityMode = 0;  // unit: 0=mAh, 1=mWh, 2=%

// Physical parameters
uint16_t iVoltage =1499; // centiVolt
uint16_t iRunTimeToEmpty = 0; // maps to BatteryEstimatedTime on Windows
uint16_t iManufacturerDate = 0; // initialized in setup function
int16_t  iCycleCount = 41;
uint16_t iTemperature = 300; // degrees Kelvin

// Parameters for ACPI compliancy
const uint16_t iDesignCapacity = 58003*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)
uint16_t iRemnCapacityLimit = iDesignCapacity/20; // critical at 5% (maps to DefaultAlert1 on Windows)
uint16_t iWarnCapacityLimit = iDesignCapacity/10; // low  at 10% (maps to DefaultAlert2 on Windows)
uint16_t iFullChargeCapacity = 40690*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)

uint16_t iRemaining[MAX_BATTERIES] = {}; // remaining charge
uint16_t iPrevRemaining=0;

HIDPowerDevice_ PowerDevice[MAX_BATTERIES];


void setup() {
#ifdef CDC_ENABLED
  Serial.begin(57600);
#endif

  for (int i = 0; i < MAX_BATTERIES; i++) {
    // initialize batteries with 30% charge
    iRemaining[i] = 0.30f*iFullChargeCapacity;

    PowerDevice[i].SetManufacturer(STRING_OEMVENDOR);
    PowerDevice[i].SetSerial(STRING_SERIAL);
  }

  pinMode(LED_BUILTIN, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running.

  for (int i = 0; i < MAX_BATTERIES; i++) {
    PowerDevice[i].SetFeature(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    PowerDevice[i].SetFeature(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));

    PowerDevice[i].SetFeature(HID_PD_CAPACITYMODE, &bCapacityMode, sizeof(bCapacityMode));
    PowerDevice[i].SetFeature(HID_PD_TEMPERATURE, &iTemperature, sizeof(iTemperature));
    PowerDevice[i].SetFeature(HID_PD_VOLTAGE, &iVoltage, sizeof(iVoltage));

    PowerDevice[i].SetStringIdxFeature(HID_PD_IDEVICECHEMISTRY, &bDeviceChemistry, STRING_DEVICECHEMISTRY);

    PowerDevice[i].SetFeature(HID_PD_DESIGNCAPACITY, &iDesignCapacity, sizeof(iDesignCapacity));
    PowerDevice[i].SetFeature(HID_PD_FULLCHRGECAPACITY, &iFullChargeCapacity, sizeof(iFullChargeCapacity));
    PowerDevice[i].SetFeature(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));
    PowerDevice[i].SetFeature(HID_PD_REMNCAPACITYLIMIT, &iRemnCapacityLimit, sizeof(iRemnCapacityLimit));
    PowerDevice[i].SetFeature(HID_PD_WARNCAPACITYLIMIT, &iWarnCapacityLimit, sizeof(iWarnCapacityLimit));

    uint16_t year = 2024, month = 10, day = 12;
    iManufacturerDate = (year - 1980)*512 + month*32 + day; // from 4.2.6 Battery Settings in "Universal Serial Bus Usage Tables for HID Power Devices"
    PowerDevice[i].SetFeature(HID_PD_MANUFACTUREDATE, &iManufacturerDate, sizeof(iManufacturerDate));

    PowerDevice[i].SetFeature(HID_PD_CYCLE_COUNT, &iCycleCount, sizeof(iCycleCount));
  }
}

void loop() {
  // propagate charge level from first to last battery
  for (int i = MAX_BATTERIES-1; i > 0; i--)
    iRemaining[i] = iRemaining[i-1];

#ifdef ENABLE_POTENTIOMETER
  // read charge level from potentiometer
  int iBattSoc = analogRead(PIN_A7); // potentiometer value in [0,1024)
  iRemaining[0] = (uint16_t)(round((float)iFullChargeCapacity*iBattSoc/1024));

  if (iRemaining[0] > iPrevRemaining + 1) // add a bit hysteresis
    iPresentStatus.Charging = true;
  else if (iRemaining[0] < iPrevRemaining - 1) // add a bit hysteresis
    iPresentStatus.Charging = false;
#else
  // simulate charge & discharge cycles
  if (iPresentStatus.Charging) {
    iRemaining[0] += 0.02f*iFullChargeCapacity; // incr. 2%

    if (iRemaining[0] > iFullChargeCapacity) {
      iRemaining[0] = iFullChargeCapacity; // clamp at 100%
      iPresentStatus.Charging = false;
    }
  } else {
    iRemaining[0] -= 0.02f*iFullChargeCapacity; // decr. 2%

    if (iRemaining[0] < 0.20f*iFullChargeCapacity) {
      iRemaining[0] = 0.20f*iFullChargeCapacity; // clamp at 20% to prevent triggering shutdown
      iPresentStatus.Charging = true;
      iCycleCount += 1;
    }
  }
#endif

  uint16_t iAvgTimeToEmpty = 7200;
  iRunTimeToEmpty = (uint16_t)round((float)iAvgTimeToEmpty*iRemaining[0]/iFullChargeCapacity);

  // Charging
  iPresentStatus.ACPresent = iPresentStatus.Charging; // assume charging implies AC present
  // Discharging
  if(!iPresentStatus.Charging) { // assume not charging implies discharging
    iPresentStatus.Discharging = 1;
  } else {
    iPresentStatus.Discharging = 0;
  }

  // Shutdown imminent
  if(iRunTimeToEmpty < 60) {
    iPresentStatus.ShutdownImminent = 1;
#ifdef CDC_ENABLED
    Serial.println("shutdown imminent");
#endif
  } else {
    iPresentStatus.ShutdownImminent = 0;
  }

  //************ Delay ****************************************
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off;

  //************ Bulk send or interrupt ***********************
  int res = 0;
  for (int i = 0; i < MAX_BATTERIES; i++) {
    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));

    if((res >= 0) && !iPresentStatus.Charging)
      res = PowerDevice[i].SendReport(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));

    if (res >= 0)
      PowerDevice[i].SetFeature(HID_PD_TEMPERATURE, &iTemperature, sizeof(iTemperature));

    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_CYCLE_COUNT, &iCycleCount, sizeof(iCycleCount));
  }

  iPrevRemaining = iRemaining[0];

#ifdef CDC_ENABLED
  Serial.print("Remaining charge: ");
  Serial.println(iRemaining[0]);

  Serial.print("SendReport res=");
  Serial.println(res);
#endif
}
