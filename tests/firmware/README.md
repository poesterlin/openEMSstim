# Firmware tests

Host-side tests for the command parsing and safety limits in
`arduino-openEMSstim` (EMSSystem, EMSChannel, AD5252 and EMSUtil). The Arduino
core and the Wire library are replaced by small mocks in `mock/`; the Wire mock
behaves like an AD5252 and can simulate I2C failures.

    ./run.sh

Needs a C++11 compiler (`CXX` overrides the default `c++`). The sketch itself
(Bluetooth, serial routing) is not covered; CI compiles it for the Nano.
