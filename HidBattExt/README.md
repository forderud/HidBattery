# Windows HidBatt extension driver
`HidBattExt` driver that extends the in-built `HidBatt` driver in Windows to also parse and report `CycleCount` and `Temperature` battery parameters.

The HidBatt drivers inability to report these parameters have already been reported to Microsoft in https://aka.ms/AAu4w9g and https://aka.ms/AAu4w8p . The driver will no longer be needed if or when Microsoft improves their `HidBatt` driver.

