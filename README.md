# Arduino battery emulator
Make an Arduino board with USB capabilities act as a [HID Power Device](https://www.usb.org/sites/default/files/pdcv11.pdf) to emulate one or more battery packs. This can be useful for simulating various power scenarios that are otherwise hard to test with an actual battery.

The project is based on Alexander Bratchik's [HIDPowerDevice](https://github.com/abratchik/HIDPowerDevice) project, but with changes to make the emulated battery more like a "battery" instead of a "UPS". The project is also extended to emulate multiple batteries.

## Supported Arduinos
* (Pro)[Micro](https://docs.arduino.cc/hardware/micro/)
* Leonardo

## Setup & Usage
Clone this repository to your Arduino libraries folder (`C:\Users\<username>\Documents\Arduino\libraries\HidBattery` on Windows). Then, open the `battery/battery.ino` sketch in Arduino IDE and press "Upload". The Operating System will afterwards detect one or more new batteries.

The [`BatteryQuery.exe`](https://github.com/forderud/BatterySimulator) tool can be used for querying battery parameters from the command line.

### Additional setup on Linux
Copy `linux/98-upower-hid.rules` file to the `/etc/udev/rules.d/` folder and reboot. This is required for Linux device manager (udev) to recognize the Arduino board as a battery. 

## Tested on the following Operating Systems
* Mac OS 14 Sonoma
* Ubuntu 24 LTS 
* Windows 2000 - 11

### Screenshots
#### Windows 11
![image](https://github.com/user-attachments/assets/3e878c38-26f7-47d4-83f7-550039ceaae2)  
Obtained with `CDC_DISABLED` defined[^1] to emulate 6 batteries.

#### Windows 10
![image](https://github.com/user-attachments/assets/1ed60c05-b280-4781-a16f-40c1f56f2a1c)  
Obtained with `CDC_DISABLED` defined[^1] to emulate 6 batteries.

#### Windows 2000
![image](https://github.com/user-attachments/assets/e1bae398-1769-468a-80fb-083cc57f32b3)  
Obtained with `CDC_DISABLED` defined[^1] to emulate 6 batteries.

[^1]: `CDC_DISABLED` can be defined by uncommenting a line in `%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\<version>\cores\arduino\USBDesc.h`. This will disable serial console over USB, which will free up 3 USB endpoints for alternative usage.

### MacOS
![image](https://github.com/user-attachments/assets/ec24ff0f-f7c7-46ef-9882-22ae3cd5c4bf)  
Limitation: MacOS displays the battery as a UPS. The UI furthermore only display aggregated information, and not per-battery.

### Linux
![image](https://github.com/user-attachments/assets/26d1babd-27d4-40c8-beef-d3f7f88c0dc1)  
Limtation: Linux seem to assume charge values in `%`, regardless of the actual unit ([upower issue #300](https://gitlab.freedesktop.org/upower/upower/-/issues/300)).


# Windows HidBatt extension driver
This repo also contains a [`HidBattExt`](HidBattExt/) filter driver that extends the in-built `HidBatt` driver in Windows to also parse and report `CycleCount` and `Temperature` battery parameters.
