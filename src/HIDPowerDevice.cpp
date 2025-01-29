/*
  HIDPowerDevice.cpp

  Copyright (c) 2020, Aleksandr Bratchik, Fredrik Orderud

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "HIDPowerDevice.h"

//================================================================================
//================================================================================
//	UPS

static const uint8_t s_hidReportDescriptor[] PROGMEM = {

    0x05, 0x84, // USAGE_PAGE (Power Device)
    0x09, 0x04, // USAGE (UPS)
    0xA1, 0x01, // COLLECTION (Application)
    0x09, 0x24, //   USAGE (Sink)
    0xA1, 0x02, //   COLLECTION (Logical)
    0x75, 0x08, //     REPORT_SIZE (8)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00, //     LOGICAL_MAXIMUM (255)
    0x85, HID_PD_IPRODUCT, //     REPORT_ID (1)
    0x09, 0xFE, //     USAGE (iProduct)
    0x79, IPRODUCT, //     STRING INDEX (2)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_SERIAL, //     REPORT_ID (2)
    0x09, 0xFF, //     USAGE (iSerialNumber)
    0x79, ISERIAL, //  STRING INDEX (3)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_MANUFACTURER, //     REPORT_ID (3)
    0x09, 0xFD, //     USAGE (iManufacturer)
    0x79, IMANUFACTURER, //     STRING INDEX (1)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x05, 0x85, //     USAGE_PAGE (Battery System) ====================
    0x85, HID_PD_IDEVICECHEMISTRY, //     REPORT_ID (4)
    0x09, 0x89, //     USAGE (iDeviceChemistry)
    0x79, IDEVICECHEMISTRY, //     STRING INDEX (4)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_CAPACITYMODE, //     REPORT_ID (22)
    0x09, 0x2C, //     USAGE (CapacityMode)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_CPCTYGRANULARITY1, //     REPORT_ID (16)
    0x09, 0x8D, //     USAGE (CapacityGranularity1)
    0x75, 0x10, //     REPORT_SIZE (16)
    0x95, 0x01, //     REPORT_COUNT (1)
    0x67, 0x01, 0x10, 0x10, 0x00, //     UNIT (AmpSec)
    0x55, 0x00, //     UNIT_EXPONENT (0)
    0xB1, 0x22, //     FEATURE (Data, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_CPCTYGRANULARITY2, //     REPORT_ID (24)
    0x09, 0x8E, //     USAGE (CapacityGranularity2)
    0xB1, 0x23, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Nonvolatile, Bitfield)

    0x85, HID_PD_FULLCHRGECAPACITY, //     REPORT_ID (14)
    0x09, 0x67, //     USAGE (FullChargeCapacity)
    0x95, 0x01, //     REPORT_COUNT (1)
    0xB1, 0x83, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_DESIGNCAPACITY, //     REPORT_ID (23)
    0x09, 0x83, //     USAGE (DesignCapacity)
    0xB1, 0x83, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_REMAININGCAPACITY, //     REPORT_ID (12)
    0x09, 0x66, //     USAGE (RemainingCapacity)
    0x81, 0xA3, //     INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x66, //     USAGE (RemainingCapacity)
    0xB1, 0xA3, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_REMNCAPACITYLIMIT, //     REPORT_ID (15)
    0x09, 0x29, //     USAGE (RemainingCapacityLimit)
    0xB1, 0xA2, //     FEATURE (Data, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_WARNCAPACITYLIMIT, //     REPORT_ID (17)
    0x09, 0x8C, //     USAGE (WarningCapacityLimit)
    0xB1, 0xA2, //     FEATURE (Data, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_MANUFACTUREDATE, //     REPORT_ID (9)
    0x09, 0x85, //     USAGE (ManufacturerDate)
    0x75, 0x10, //     REPORT_SIZE (16)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     LOGICAL_MAXIMUM (65534)
    0xB1, 0xA3, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x85, HID_PD_RUNTIMETOEMPTY, //     REPORT_ID (13)
    0x09, 0x68, //     USAGE (RunTimeToEmpty)
    0x81, 0xA3, //     INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x68, //     USAGE (RunTimeToEmpty)
    0xB1, 0xA3, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x05, 0x84, //     USAGE_PAGE (Power Device) ====================
    0x85, HID_PD_VOLTAGE, //     REPORT_ID (11)
    0x09, 0x30, //     USAGE (Voltage)
    0x15, 0x00, //     LOGICAL_MINIMUM (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     LOGICAL_MAXIMUM (65535)
    0x67, 0x21, 0xD1, 0xF0, 0x00, //     UNIT (Centivolts)
    0x55, 0x05, //     UNIT_EXPONENT (5)
    0x81, 0xA3, //     INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x30, //     USAGE (Voltage)
    0xB1, 0xA3, //     FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)

    0x09, 0x02, //     USAGE (PresentStatus)
    0xA1, 0x02, //     COLLECTION (Logical)
    0x85, HID_PD_PRESENTSTATUS, //       REPORT_ID (7)
    0x05, 0x85, //       USAGE_PAGE (Battery System) =================
    0x09, 0x44, //       USAGE (Charging)
    0x75, 0x01, //       REPORT_SIZE (1)
    0x15, 0x00, //       LOGICAL_MINIMUM (0)
    0x25, 0x01, //       LOGICAL_MAXIMUM (1)
    0x81, 0xA3, //       INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x44, //       USAGE (Charging)
    0xB1, 0xA3, //       FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)
    0x09, 0x45, //       USAGE (Discharging)
    0x81, 0xA3, //       INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x45, //       USAGE (Discharging)
    0xB1, 0xA3, //       FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)
    0x09, 0xD0, //       USAGE (ACPresent)
    0x81, 0xA3, //       INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0xD0, //       USAGE (ACPresent)
    0xB1, 0xA3, //       FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)
    0x05, 0x84, //       USAGE_PAGE (Power Device) =================
    0x09, 0x69, //       USAGE (ShutdownImminent)
    0x81, 0xA3, //       INPUT (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Bitfield)
    0x09, 0x69, //       USAGE (ShutdownImminent)
    0xB1, 0xA3, //       FEATURE (Constant, Variable, Absolute, No Wrap, Linear, No Preferred, No Null Position, Volatile, Bitfield)
    0x95, 0x04, //       REPORT_COUNT (4) // padding bits to make the report byte aligned
    0x81, 0x01, //       INPUT (Constant, Array, Absolute)
    0xB1, 0x01, //       FEATURE (Constant, Array, Absolute, No Wrap, Linear, Preferred State, No Null Position, Nonvolatile, Bitfield)
    0xC0,       //     END_COLLECTION
    0xC0,       //   END_COLLECTION
    0xC0        // END_COLLECTION
};

HIDPowerDevice_::HIDPowerDevice_() {
    static const HIDSubDescriptor node(s_hidReportDescriptor, sizeof (s_hidReportDescriptor));

    AppendDescriptor(&node);

    SetFeature(HID_PD_IPRODUCT, &bProduct, sizeof(bProduct)); // automatically populated with "Arduino Micro"
}

int HIDPowerDevice_::SetManufacturer(const char* data) {
    return SetStringIdxFeature(HID_PD_MANUFACTURER, &bManufacturer, data);
}

int HIDPowerDevice_::SetSerial(const char* data) {
    return SetStringIdxFeature(HID_PD_SERIAL, &bSerial, data);
}

int HIDPowerDevice_::SetStringIdxFeature(uint8_t id, const uint8_t* index, const char* data) {
    // set string index for report
    int res = SetFeature(id, index, 1);
    if(res <= 0)
        return res;

    // set string at given index
    res = SetString(*index , data);
    return res;
}
