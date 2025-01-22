# Arduino battery emulator
Make an Arduino board with USB capabilities act as a [HID Power Device](https://www.usb.org/sites/default/files/pdcv11.pdf) to emulate one or more battery packs. This can be useful for simulating various power scenarios that are otherwise hard to test with an actual battery.

The project is based on Alexander Bratchik's [HIDPowerDevice](https://github.com/abratchik/HIDPowerDevice) project, but with changes to make the emulated battery more like a "battery" instead of a "UPS". The project is also extended to emulate multiple batteries.

## Supported Arduinos
* (Pro)Micro
* Leonardo

## Setup & Usage
Clone this repository to your Arduino libraries folder (`C:\Users\<username>\Documents\Arduino\libraries` on Windows). Then, open the `examples/UPS/UPS.ino` sketch in Arduino IDE and press "Upload". The Operating System will afterwards detect one or more new batteries.

### Additional setup on Linux
Copy `linux/98-upower-hid.rules` file to the `/etc/udev/rules.d/` folder and reboot. This is required for Linux device manager (udev) to recognize the Arduino board as UPS. 

## Tested on the following Operating Systems
* Mac OS 14 Sonoma
* Ubuntu 24 LTS 
* Windows 2000 - 11

### Screenshots
Windows 2000:  
![image](https://github.com/user-attachments/assets/e1bae398-1769-468a-80fb-083cc57f32b3)

## License
Copyright (c) Alex Bratchik 2020. All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
