Adapter to use a Teensy4.0 board with the simpBMS in place of the Teensy3.2.

Mostly all pins are the same, but CAN1 must be rerouted. And the Teensy4.0 UART3 is used as Teensy3.2 UART2.
The firmware is modified to take the rerouting into account.
The source code modifications are under the USING_TEENSY4 directive.

The schematic is drawn with kicad 7.0 and under the Apache licence

Note that this is just a proof of concept, the schematics of the carrier board could be adapted to add a CAN bus or two.
