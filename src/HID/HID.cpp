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

#include "HID.h"

HID_::HID_() : PluggableUSBModule(1, 1, m_epType) {
    m_epType[0] = EP_TYPE_INTERRUPT_IN;
    PluggableUSB().plug(this);
}

int HID_::getInterface(uint8_t* interfaceCount)
{
    *interfaceCount += 1; // uses 1
    HIDDescriptor hidInterface = {
        D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
        D_HIDREPORT(m_descriptorSize),
        D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 0x14)
    };
    return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
}

/** Send a USB descriptor string.
  The string is converted from ASCII to UTF-16 with 2-byte prefix.
  Copied from https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/USBCore.cpp */
static bool USB_SendStringDescriptor(const char* string_P, u8 string_len, uint8_t flags) {
    u8 c[2] = {string_len*2 + 2, // descriptor size
               0x03};            // string descriptor type
    USB_SendControl(0, &c, 2);

    bool pgm = flags & TRANSFER_PGM;
    for(u8 i = 0; i < string_len; i++) {
        // expand from ASCII to UTF-16
        c[0] = pgm ? pgm_read_byte(&string_P[i]) : string_P[i];
        c[1] = 0;
        int r = USB_SendControl(0, &c, 2);
        if(!r)
            return false;
    }
    return true;
}

int HID_::getDescriptor(USBSetup& setup)
{
    if (setup.bRequest != GET_DESCRIPTOR) // redundant check, since it's already done before calling this method
        return 0;
    
    if (setup.bmRequestType == REQUEST_DEVICETOHOST | REQUEST_STANDARD | REQUEST_DEVICE) {
        if (setup.wIndex != 0x0409) // English (matches STRING_LANGUAGE in arduino/USBCore.cpp)
            return 0;
        
        // HID-specific strings
        if(setup.wValueH == USB_STRING_DESCRIPTOR_TYPE) {
            HIDReport* rep = GetFeature(setup.wValueL, true/*string*/);
            if(rep)
                return USB_SendStringDescriptor((char*)rep->data, strlen_P((char*)rep->data), TRANSFER_PGM);
        }
        return 0;
    } else if (setup.bmRequestType == REQUEST_DEVICETOHOST_STANDARD_INTERFACE) {
        // Check if this is a HID Class Descriptor request

        if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE)
            return 0;

        // In a HID Class Descriptor wIndex cointains the interface number
        if (setup.wIndex != pluggedInterface)
            return 0;

        int total = 0;
        for (const HIDSubDescriptor* node = m_rootNode; node; node = node->next) {
            int res = USB_SendControl(TRANSFER_PGM, node->data, node->length);
            if (res == -1)
                return -1;
            total += res;
        }

        // Reset the protocol on reenumeration. Normally the host should not assume the state of the protocol
        // due to the USB specs, but Windows and Linux just assumes its in report mode.
        m_protocol = HID_REPORT_PROTOCOL;

        return total;
    }
    
    return 0;
}

uint8_t HID_::getShortName(char *name)
{
    // default serial number
    name[0] = 'H';
    name[1] = 'I';
    name[2] = 'D';
    name[3] = 'A' + (m_descriptorSize & 0x0F);
    name[4] = 'A' + ((m_descriptorSize >> 4) & 0x0F);
    return 5;
}

void HID_::AppendDescriptor(const HIDSubDescriptor *node)
{
    if (!m_rootNode) {
        m_rootNode = node;
    } else {
        HIDSubDescriptor *current = m_rootNode;
        while (current->next)
            current = current->next;

        current->next = node;
    }
    m_descriptorSize += node->length;
}

void HID_::SetFeature(uint8_t id, const void* data, int len)
{
    SetFeatureInternal(id, false/*string*/, data, len);
}

void HID_::SetString(const uint8_t index, const char* data)
{
    SetFeatureInternal(index, true/*string*/, data, strlen_P(data));
}

void HID_::SetFeatureInternal(uint8_t id, bool str, const void* data, int len)
{
    if(!m_rootReport) {
        m_rootReport = new HIDReport(id, str, data, len);
        return;
    }

    for (HIDReport* current = m_rootReport; current; current = current->next) {
        if((current->id == id) && (current->str == str))
            return; // feature already configured

        if(!current->next) {
            // append at the end
            current->next = new HIDReport(id, str, data, len);
            break;
        }
    }
}

int HID_::SendReport(uint8_t id, const void* data, int len)
{
    auto ret = USB_Send(pluggedEndpoint, &id, 1);
    if (ret < 0)
        return ret;

    auto ret2 = USB_Send(pluggedEndpoint | TRANSFER_RELEASE, data, len);
    if (ret2 < 0)
        return ret2;
    return ret + ret2;
}

const HIDReport* HID_::GetFeature(uint8_t id, bool str)
{
    for(const HIDReport* current=m_rootReport; current; current=current->next) {
        if((id == current->id) && (str == current->str))
            return current;
    }
    return nullptr;
}

bool HID_::setup(USBSetup& setup)
{
    if (setup.wIndex != pluggedInterface)
        return false;

    if (setup.bmRequestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE) {
        if (setup.bRequest == HID_GET_REPORT) {
            if(setup.wValueH == HID_REPORT_TYPE_FEATURE) {
                HIDReport* current = GetFeature(setup.wValueL, false/*string*/);
                if(current){
                    int res = USB_SendControl(0, &(current->id), 1);
                    if(res > 0)
                        res = USB_SendControl(0, current->data, current->length);
                    return (res > 0);
                }

                return false;
            }
            return true;
        }
        if (setup.bRequest == HID_GET_PROTOCOL) {
            // TODO: Send8(m_protocol);
            return true;
        }
        if (setup.bRequest == HID_GET_IDLE) {
            // TODO: Send8(m_idle);
        }
    }

    if (setup.bmRequestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
        if (setup.bRequest == HID_SET_PROTOCOL) {
            // The USB Host tells us if we are in boot or report mode.
            // This only works with a real boot compatible device.
            m_protocol = setup.wValueL;
            return true;
        }
        if (setup.bRequest == HID_SET_IDLE) {
            m_idle = setup.wValueL;
            return true;
        }
        if (setup.bRequest == HID_SET_REPORT) {
            if(setup.wValueH == HID_REPORT_TYPE_FEATURE) {
                HIDReport* current = GetFeature(setup.wValueL, false/*string*/);
                if(!current)
                    return false;

                if(setup.wLength != current->length + 1)
                    return false;

                uint8_t* data = new uint8_t[setup.wLength];
                USB_RecvControl(data, setup.wLength);
                if(*data != current->id)
                    return false;
                memcpy((uint8_t*)current->data, data+1, current->length);
                delete[] data;
                return true;
            }
        }
    }

    return false;
}
