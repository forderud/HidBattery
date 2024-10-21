#include <HIDPowerDevice.h>

#define MINUPDATEINTERVAL   26
#define CHGDCHPIN           4
#define RUNSTATUSPIN        5
#define COMMLOSTPIN         10
#define BATTSOCPIN          A7

int iIntTimer=0;


// String constants
const char STRING_DEVICECHEMISTRY[] PROGMEM = "LiP";
const byte bDeviceChemistry = IDEVICECHEMISTRY;

const char STRING_OEMVENDOR[] PROGMEM = "BatteryVendor";

const char STRING_SERIAL[] PROGMEM = "1234";

PresentStatus iPresentStatus = {}, iPreviousStatus = {};

byte bCapacityMode = 0;  // unit: 0=mAh, 1=mWh, 2=%

// Physical parameters
uint16_t iVoltage =1499; // centiVolt
uint16_t iManufacturerDate = 0; // initialized in setup function

// Parameters for ACPI compliancy
const uint32_t iDesignCapacity = 58003*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)
uint32_t iFullChargeCapacity = 40690*360/iVoltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)

uint32_t iRemaining[BATTERY_COUNT] = {}; // remaining charge
uint32_t iPrevRemaining=0;
bool bCharging = false;

int iRes=0;


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

  pinMode(CHGDCHPIN, INPUT_PULLUP); // ground this pin to simulate power failure. 
  pinMode(RUNSTATUSPIN, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running. 
  pinMode(COMMLOSTPIN, OUTPUT); // output is on once communication is lost with the host, otherwise off.

  for (int i = 0; i < BATTERY_COUNT; i++) {
    PowerDevice[i].SetFeature(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    PowerDevice[i].SetFeature(HID_PD_CAPACITYMODE, &bCapacityMode, sizeof(bCapacityMode));
    PowerDevice[i].SetFeature(HID_PD_VOLTAGE, &iVoltage, sizeof(iVoltage));

    PowerDevice[i].setStringFeature(HID_PD_IDEVICECHEMISTRY, &bDeviceChemistry, STRING_DEVICECHEMISTRY);
    PowerDevice[i].SetFeature(0xFF00 | PowerDevice[i].bManufacturer, STRING_OEMVENDOR, strlen_P(STRING_OEMVENDOR));

    PowerDevice[i].SetFeature(HID_PD_DESIGNCAPACITY, &iDesignCapacity, sizeof(iDesignCapacity));
    PowerDevice[i].SetFeature(HID_PD_FULLCHRGECAPACITY, &iFullChargeCapacity, sizeof(iFullChargeCapacity));
    PowerDevice[i].SetFeature(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));

    uint16_t year = 2024, month = 10, day = 12;
    iManufacturerDate = (year - 1980)*512 + month*32 + day; // from 4.2.6 Battery Settings in "Universal Serial Bus Usage Tables for HID Power Devices"
    PowerDevice[i].SetFeature(HID_PD_MANUFACTUREDATE, &iManufacturerDate, sizeof(iManufacturerDate));
  }
}

void loop() {
  //*********** Measurements Unit ****************************
  int iBattSoc = analogRead(BATTSOCPIN); // potensiometer value in [0,1024)

  for (int i = BATTERY_COUNT-1; i > 0; i--)
    iRemaining[i] = iRemaining[i-1]; // propagate charge level from first to last battery
  iRemaining[0] = (uint32_t)(round((float)iFullChargeCapacity*iBattSoc/1024));

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
  } else {
    iPresentStatus.Discharging = 0;
    iPresentStatus.RemainingTimeLimitExpired = 0;
  }

  iPresentStatus.BatteryPresent = 1;


  //************ Delay ****************************************
  delay(1000);
  iIntTimer++;
  digitalWrite(RUNSTATUSPIN, HIGH);   // turn the LED on (HIGH is the voltage level);
  delay(1000);
  iIntTimer++;
  digitalWrite(RUNSTATUSPIN, LOW);   // turn the LED off;

  //************ Bulk send or interrupt ***********************

  if((iPresentStatus != iPreviousStatus) || (iRemaining[0] != iPrevRemaining) || (iIntTimer>MINUPDATEINTERVAL) ) {
    for (int i = 0; i < BATTERY_COUNT; i++) {
      PowerDevice[i].SendReport(HID_PD_REMAININGCAPACITY, &iRemaining[i], sizeof(iRemaining[i]));

      iRes = PowerDevice[i].SendReport(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));
    }

    if(iRes <0 )
      digitalWrite(COMMLOSTPIN, HIGH);
    else
      digitalWrite(COMMLOSTPIN, LOW);

    iIntTimer = 0;
    iPreviousStatus = iPresentStatus;
    iPrevRemaining = iRemaining[0];
  }

#ifdef CDC_ENABLED
  Serial.println(iRemaining[0]);
  Serial.println(iRes);
#endif
}
