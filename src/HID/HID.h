/*
  Copyright (c) Arduino LLC, Peter Barrett, Aleksandr Bratchik, Fredrik Orderud

  Permission to use, copy, modify, and/or distribute this software for
  any purpose with or without fee is hereby granted, provided that the
  above copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
  WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
  BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
  OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
  SOFTWARE.
 */
#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <PluggableUSB.h>

// HID 'Driver'
// ------------
#define HID_GET_REPORT        0x01
#define HID_GET_IDLE          0x02
#define HID_GET_PROTOCOL      0x03
#define HID_SET_REPORT        0x09
#define HID_SET_IDLE          0x0A
#define HID_SET_PROTOCOL      0x0B

#define HID_HID_DESCRIPTOR_TYPE         0x21
#define HID_REPORT_DESCRIPTOR_TYPE      0x22
#define HID_PHYSICAL_DESCRIPTOR_TYPE    0x23

// HID subclass HID1.11 Page 8 4.2 Subclass
#define HID_SUBCLASS_NONE 0

// HID Keyboard/Mouse bios compatible protocols HID1.11 Page 9 4.3 Protocols
#define HID_PROTOCOL_NONE 0

// Normal or bios protocol (Keyboard/Mouse) HID1.11 Page 54 7.2.5 Get_Protocol Request
// "protocol" variable is used for this purpose.
#define HID_BOOT_PROTOCOL	0
#define HID_REPORT_PROTOCOL	1

// HID Request Type HID1.11 Page 51 7.2.1 Get_Report Request
#define HID_REPORT_TYPE_INPUT   1
#define HID_REPORT_TYPE_OUTPUT  2
#define HID_REPORT_TYPE_FEATURE 3

struct HIDDescDescriptor {
  uint8_t len;      // 9
  uint8_t dtype;    // 0x21
  uint8_t addr;
  uint8_t versionL; // 0x101
  uint8_t versionH; // 0x101
  uint8_t country;
  uint8_t desctype; // 0x22 report
  uint8_t descLenL;
  uint8_t descLenH;
};

struct HIDDescriptor {
  InterfaceDescriptor hid;
  HIDDescDescriptor   desc;
  EndpointDescriptor  in;
};

class HIDReport {
public:
    HIDReport(uint8_t i, bool s, const void *d, uint8_t l) : id(i), str(s), data(d), length(l) {}
    
    uint8_t id;
    bool str;
    const void* data;
    uint16_t length;
    const HIDReport *next = NULL;
};

struct HIDReportDescriptor {
  void* data = nullptr;
  uint16_t length = 0;
};

class HID_ : public PluggableUSBModule {
public:
    HID_();

    int SendReport(uint8_t id, const void* data, int len);

    /** The "data" pointer need to outlast this object. */ 
    void SetFeature(uint8_t id, const void* data, int len);

protected:
    /** The "data" pointer need to outlast this object. */ 
    void SetString(const uint8_t index, const char* data);
    
    /** The "node" pointer need to outlast this object. */ 
    void SetDescriptor(const void *data, uint16_t length);
    
    // Implementation of the PluggableUSBModule
    int getInterface(uint8_t* interfaceCount) override;
    int getDescriptor(USBSetup& setup) override;
    bool setup(USBSetup& setup) override;
    uint8_t getShortName(char* name) override;
    
private:
    const HIDReport* GetFeature(uint8_t id, bool str);
    
    void SetFeatureInternal(uint8_t id, bool str, const void* data, int len);

    uint8_t m_epType[1];

    HIDReportDescriptor m_reportDesc;

    uint8_t m_protocol = HID_REPORT_PROTOCOL;
    uint8_t m_idle = 1;
  
    // Buffer pointer to hold the feature data
    HIDReport* m_rootReport = nullptr;
};

#define D_HIDREPORT(length) { 9, 0x21, 0x01, 0x01, 0, 1, 0x22, lowByte(length), highByte(length) }
