#pragma once
#include <HID/HID.h>

#define HID_PD_IPRODUCT              0x01 // FEATURE ONLY
#define HID_PD_SERIAL                0x02 // FEATURE ONLY
#define HID_PD_MANUFACTURER          0x03 // FEATURE ONLY

#define HID_PD_IDEVICECHEMISTRY      0x04 // Feature

#define HID_PD_PRESENTSTATUS         0x07 // INPUT OR FEATURE(required by Windows)
#define HID_PD_MANUFACTUREDATE       0x09 // FEATURE ONLY
#define HID_PD_TEMPERATURE           0x0A // 10 INPUT OR FEATURE
#define HID_PD_VOLTAGE               0x0B // 11 INPUT (NA) OR FEATURE(implemented)
#define HID_PD_REMAININGCAPACITY     0x0C // 12 INPUT OR FEATURE(required by Windows)
#define HID_PD_RUNTIMETOEMPTY        0x0D // 13 INPUT OR FEATURE (maps to BatteryEstimatedTime on Windows)
#define HID_PD_FULLCHRGECAPACITY     0x0E // 14 FEATURE ONLY. Last Full Charge Capacity
#define HID_PD_DESIGNCAPACITY        0x0F // 15 FEATURE ONLY
#define HID_PD_REMNCAPACITYLIMIT     0x10 // 16 FEATURE ONLY (maps to DefaultAlert1 on Windows)
#define HID_PD_WARNCAPACITYLIMIT     0x11 // 17 FEATURE ONLY (maps to DefaultAlert2 on Windows)
#define HID_PD_CYCLE_COUNT           0x14 // 20 INPUT OR FEATURE
#define HID_PD_CAPACITYMODE          0x16 // 22 FEATURE ONLY

// PresentStatus dynamic flags
struct PresentStatus {
  uint8_t Charging : 1;                   // bit 0x00
  uint8_t Discharging : 1;                // bit 0x01
  uint8_t ACPresent : 1;                  // bit 0x02
  uint8_t ShutdownImminent : 1;           // bit 0x03 (maps to BATTERY_CRITICAL on Windows)
  uint8_t unused1 : 1;
  uint8_t unused2 : 1;
  uint8_t unused3 : 1;
  uint8_t unused4 : 1;

  operator uint8_t () {
      return *(uint8_t*)(this); // switch to std::bit_cast after migrating to C++20
  }
};
static_assert(sizeof(PresentStatus) == sizeof(uint8_t));


class HIDPowerDevice_ : public HID_ {
public:
  HIDPowerDevice_();
  
  /** The "index" & "data" pointers need to outlast this object. */ 
  void SetStringFeature(uint8_t id, const uint8_t* index, const char* data);
  
private:
  static const byte s_productIdx;
};

// max number of batteries supported by the HW
#define MAX_BATTERIES (USB_ENDPOINTS - CDC_FIRST_ENDPOINT - CDC_ENPOINT_COUNT) // 3 by default; 6 if defining CDC_DISABLED in %LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\<version>\cores\arduino\USBDesc.h
