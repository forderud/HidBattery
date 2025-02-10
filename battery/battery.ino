#include <HIDPowerDevice.h>
//#define ENABLE_POTENTIOMETER // uncomment to enable potentiometer

// String constants
const char STRING_DEVICECHEMISTRY[] PROGMEM = "LiP";
const byte DeviceChemistryIdx = IDEVICECHEMISTRY;

const char STRING_OEMVENDOR[] PROGMEM = "BatteryVendor";

const char STRING_SERIAL[] PROGMEM = "1234";

PresentStatus PresentStatus = {};

byte CapacityMode = 0;  // unit: 0=mAh, 1=mWh, 2=%

// Physical parameters
uint16_t Voltage =1499; // centiVolt
uint16_t RunTimeToEmpty = 0; // maps to BatteryEstimatedTime on Windows
uint16_t ManufacturerDate = 0; // initialized in setup function
int16_t  CycleCount = 41;
uint16_t Temperature = 300; // degrees Kelvin

// Parameters for ACPI compliancy
const uint16_t DesignCapacity = 58003*360/Voltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)
uint16_t RemnCapacityLimit = DesignCapacity/20; // critical at 5% (maps to DefaultAlert1 on Windows)
uint16_t WarnCapacityLimit = DesignCapacity/10; // low  at 10% (maps to DefaultAlert2 on Windows)
uint16_t FullChargeCapacity = 40690*360/Voltage; // AmpSec=mWh*360/centiVolt (1 mAh = 3.6 As)

uint16_t Remaining[MAX_BATTERIES] = {}; // remaining charge
uint16_t PrevRemaining=0;

HIDPowerDevice_ PowerDevice[MAX_BATTERIES];


void setup() {
#ifdef CDC_ENABLED
  Serial.begin(57600);
#endif

  for (int i = 0; i < MAX_BATTERIES; i++) {
    // initialize batteries with 30% charge
    Remaining[i] = 0.30f*FullChargeCapacity;

    PowerDevice[i].SetManufacturer(STRING_OEMVENDOR);
    PowerDevice[i].SetSerial(STRING_SERIAL);
  }

  pinMode(LED_BUILTIN, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running.

  for (int i = 0; i < MAX_BATTERIES; i++) {
    PowerDevice[i].SetFeature(HID_PD_PRESENTSTATUS, &PresentStatus, sizeof(PresentStatus));

    PowerDevice[i].SetFeature(HID_PD_RUNTIMETOEMPTY, &RunTimeToEmpty, sizeof(RunTimeToEmpty));

    PowerDevice[i].SetFeature(HID_PD_CAPACITYMODE, &CapacityMode, sizeof(CapacityMode));
    PowerDevice[i].SetFeature(HID_PD_TEMPERATURE, &Temperature, sizeof(Temperature));
    PowerDevice[i].SetFeature(HID_PD_VOLTAGE, &Voltage, sizeof(Voltage));

    PowerDevice[i].SetStringIdxFeature(HID_PD_IDEVICECHEMISTRY, &DeviceChemistryIdx, STRING_DEVICECHEMISTRY);

    PowerDevice[i].SetFeature(HID_PD_DESIGNCAPACITY, &DesignCapacity, sizeof(DesignCapacity));
    PowerDevice[i].SetFeature(HID_PD_FULLCHRGECAPACITY, &FullChargeCapacity, sizeof(FullChargeCapacity));
    PowerDevice[i].SetFeature(HID_PD_REMAININGCAPACITY, &Remaining[i], sizeof(Remaining[i]));
    PowerDevice[i].SetFeature(HID_PD_REMNCAPACITYLIMIT, &RemnCapacityLimit, sizeof(RemnCapacityLimit));
    PowerDevice[i].SetFeature(HID_PD_WARNCAPACITYLIMIT, &WarnCapacityLimit, sizeof(WarnCapacityLimit));

    uint16_t year = 2024, month = 10, day = 12;
    ManufacturerDate = (year - 1980)*512 + month*32 + day; // from 4.2.6 Battery Settings in "Universal Serial Bus Usage Tables for HID Power Devices"
    PowerDevice[i].SetFeature(HID_PD_MANUFACTUREDATE, &ManufacturerDate, sizeof(ManufacturerDate));

    PowerDevice[i].SetFeature(HID_PD_CYCLE_COUNT, &CycleCount, sizeof(CycleCount));
  }
}

void loop() {
  // propagate charge level from first to last battery
  for (int i = MAX_BATTERIES-1; i > 0; i--)
    Remaining[i] = Remaining[i-1];

#ifdef ENABLE_POTENTIOMETER
  // read charge level from potentiometer
  int BattSoc = analogRead(PIN_A7); // potentiometer value in [0,1024)
  Remaining[0] = (uint16_t)(round((float)FullChargeCapacity*BattSoc/1024));

  if (Remaining[0] > PrevRemaining + 1) // add a bit hysteresis
    PresentStatus.Charging = true;
  else if (Remaining[0] < PrevRemaining - 1) // add a bit hysteresis
    PresentStatus.Charging = false;
#else
  // simulate charge & discharge cycles
  if (PresentStatus.Charging) {
    Remaining[0] += 0.02f*FullChargeCapacity; // incr. 2%

    if (Remaining[0] > FullChargeCapacity) {
      Remaining[0] = FullChargeCapacity; // clamp at 100%
      PresentStatus.Charging = false;
    }
  } else {
    Remaining[0] -= 0.02f*FullChargeCapacity; // decr. 2%

    if (Remaining[0] < 0.20f*FullChargeCapacity) {
      Remaining[0] = 0.20f*FullChargeCapacity; // clamp at 20% to prevent triggering shutdown
      PresentStatus.Charging = true;
      CycleCount += 1;
    }
  }
#endif

  uint16_t AvgTimeToEmpty = 7200;
  RunTimeToEmpty = (uint16_t)round((float)AvgTimeToEmpty*Remaining[0]/FullChargeCapacity);

  // Charging
  PresentStatus.ACPresent = PresentStatus.Charging; // assume charging implies AC present
  // Discharging
  if(!PresentStatus.Charging) { // assume not charging implies discharging
    PresentStatus.Discharging = 1;
  } else {
    PresentStatus.Discharging = 0;
  }

  // Shutdown imminent
  if(RunTimeToEmpty < 60) {
    PresentStatus.ShutdownImminent = 1;
#ifdef CDC_ENABLED
    Serial.println("shutdown imminent");
#endif
  } else {
    PresentStatus.ShutdownImminent = 0;
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
      res = PowerDevice[i].SendReport(HID_PD_REMAININGCAPACITY, &Remaining[i], sizeof(Remaining[i]));

    if((res >= 0) && !PresentStatus.Charging)
      res = PowerDevice[i].SendReport(HID_PD_RUNTIMETOEMPTY, &RunTimeToEmpty, sizeof(RunTimeToEmpty));

    if (res >= 0)
      PowerDevice[i].SetFeature(HID_PD_TEMPERATURE, &Temperature, sizeof(Temperature));

    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_PRESENTSTATUS, &PresentStatus, sizeof(PresentStatus));

    if (res >= 0)
      res = PowerDevice[i].SendReport(HID_PD_CYCLE_COUNT, &CycleCount, sizeof(CycleCount));
  }

  PrevRemaining = Remaining[0];

#ifdef CDC_ENABLED
  Serial.print("Remaining charge: ");
  Serial.println(Remaining[0]);

  Serial.print("SendReport res=");
  Serial.println(res);
#endif
}
