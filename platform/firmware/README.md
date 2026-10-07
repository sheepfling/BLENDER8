# Editable B8 firmware

Edit [`firmware.cpp`](firmware.cpp) to develop the fictional B8 appliance controller. This starter
deliberately leaves the motor off. Keep `reset()`, `step()`, and `vectors` as the entry points, and
include only the B8 SDK and files in this directory.

Start with [New Employee's setup route](../../NEW-EMPLOYEE-START.md), then use the
[firmware lab](../docs/FIRMWARE-LAB.md) for small register and display exercises. The current
customer behavior is in the [correspondence](../requirements/correspondence.md); the
[board wiring](../docs/BOARD_WIRING.md) maps pins and safety contacts.

`board_config.hpp` contains wiring constants, not simulator access. A passing C++ platform test
suite does not mean this firmware passes customer acceptance.
