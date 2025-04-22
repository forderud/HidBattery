# Windows HidBatt extension filter driver
`HidBattExt` driver that extends the in-built `HidBatt` driver in Windows to also parse and report `CycleCount` and `Temperature` battery parameters. The HidBatt driver also suffer from other shortcommings not addressed by HidBattExt, such as missing WMI interface and not being available in ARM64 builds of Windows.

The HidBatt drivers inability to report these parameters have already been reported to Microsoft in https://aka.ms/AAu4w9g and https://aka.ms/AAu4w8p as well as through Microsoft support, with case numbers 2504040040010238 (ARM64 inclusion) , 2504090040010178 (missing Temperature&CycleCount parameters)  & 2504090040010176 (missing WMI interfaces). This driver will no longer be needed if Microsoft improves their `HidBatt` driver.

## Description
The filter driver places itself both _above_ and _below_ HidBatt:  
![image](https://github.com/user-attachments/assets/d435277d-3bb9-46ed-8a42-392e3da676ea)

The driver instance _below_ HidBatt first filters the [HID Power Device](https://www.usb.org/sites/default/files/pdcv11.pdf) communication with the battery to pick up the missing `CycleCount` (UsagePage=0x85, Usage=0x6B) and `Temperature` (UsagePage=0x84, Usage=0x36) parameters from HID `FEATURE` and `INPUT` reports. The driver instance _above_ HidBatt afterwards filters [`IOCTL_BATTERY_QUERY_INFORMATION`](https://learn.microsoft.com/en-us/windows/win32/power/ioctl-battery-query-information) communication to include these parameters in the HidBatt responses.

## Driver testing
See [Driver testing](https://github.com/forderud/IntelliMouseDriver/wiki/Driver-testing) for an introduction to how to install and test drivers on a dedicated Windows computer with `testsigning` enabled.

The Arduino [battery.ino](/battery/battery.ino) sketch in this repo can be used for testing multi-battery setups.  
![image](https://github.com/user-attachments/assets/98040437-5968-4a44-92ac-492e858bf216)  

The [`BatteryQuery.exe`](https://github.com/forderud/BatterySimulator) tool can be used for querying battery IOCTL parameters from the command line. Please be aware that the tool will work-around the missing `CycleCount` and `Temperature` parameters by querying them from the underlying HID device. This will hide the limitation.

## Acknowledgement
The driver is based on the [TailLight](https://github.com/forderud/IntelliMouseDriver/tree/main/TailLight) driver for Microsoft Pro IntelliMouse, which again is based on Microsoft's [firefly](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample.

The filter driver design is furthermore influenced by valuable feedback from _Tim Roberts_, _Mark Roddy_ and _Doron Holan_ in the OSR [How to continuously obtain HID input reports from non-HIDClass driver](https://community.osr.com/t/how-to-continuously-obtain-hid-input-reports-from-non-hidclass-driver/59447/19) forum posting.

## Documentation updates
Windows driver documentation updates triggered by this driver:
* [Extend WdfRequestSetCompletionRoutine to mention need for request formatting](https://github.com/MicrosoftDocs/windows-driver-docs-ddi/pull/1601)
