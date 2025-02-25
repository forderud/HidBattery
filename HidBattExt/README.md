# Windows HidBatt extension driver
`HidBattExt` driver that extends the in-built `HidBatt` driver in Windows to also parse and report `CycleCount` and `Temperature` battery parameters.

The HidBatt drivers inability to report these parameters have already been reported to Microsoft in https://aka.ms/AAu4w9g and https://aka.ms/AAu4w8p . The driver will no longer be needed if or when Microsoft improves their `HidBatt` driver.


## Background
The driver is based on the [TailLight](https://github.com/forderud/IntelliMouseDriver/tree/main/TailLight) driver for Microsoft Pro IntelliMouse, which again is based on the Microsoft [firefly](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample driver.

The filter driver design is furthermore influenced by valuable feedback from Tim Roberts, Mark Roddy and Doron Holan in the OSR [How to continuously obtain HID input reports from non-HIDClass driver?](https://community.osr.com/t/how-to-continuously-obtain-hid-input-reports-from-non-hidclass-driver/59447/19) forum posting.
