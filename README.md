INTRODUCTION
This device performs an access control by means of 125 kHz RFID tags as the one shown in Figure 1. It consists of a microcontroller and a RFID reader which are communicated by means of a serial link.
![alt text](https://raw.githubusercontent.com/mizamae/SafeAccess/path/to/RFID_tag.jpg)
When an accepted RFID tag is swiped around the reader, the device grants access to a protected zone by means of switching on a relay that power on the lights. The capabilities of this device can be further increased by integrating an electro-mechanical locking mechanism to the door. This could be locked and unlocked by means of two additional digital outputs.

SPECIFICATIONS
The following table shows the general specifications for the access control system.

HARDWARE DESCRIPTION
The hardware is divided into two boards as can be seen on Figure 2. This architecture increases the safety as the mainboard (board 1) can be safely placed (or even hidden) inside the protected area whereas the human-machine interface (HMI) board (board 2) can be left exposed in the un-protected area.
The board 1 acquires all the inputs (such as door opened) and executes the algorithm activating the outputs (alarm) as required; even in the absence of board 2. This guarantees that even in the case of vandalism that may destroy the exposed part (board 2), the alarm will keep on being activated in case required.

Board 1
The board 1 comprises the microprocessor, the digital inputs (DI) and digital outputs (DO) as well as the communication links with the board 2. This board receives the input voltage (Vin) and generates the 5 Vdc required by the microprocessor.

Links with the board 2
The link with the board 2 is splitted in two connectors for the sake of routing simplicity. These can be highlighted in Figure 5. The connector highlighted in red implements the communication with the RFID module whereas the one in blue sends the power supply to the board 2 and the communication with the LCD display.
Both links can be accommodated using a single UTP8 cable. Lengths up to 2,5 m have been properly tested and validated.
The pin Tx in the red connector is not used so it should not be wired.

Digital inputs
The board includes two digital inputs that will gather the information about the status of the door (pins DOOR and DOOR1 in Figure 3) and the status of the manual key that turns the unit into manual mode (pins KEY and KEY1). The digital inputs are optocoupled as seen on Figure 6.

Digital outputs
The board includes two digital outputs that will control the status of the lights (pin LIGHT in Figure 3) and the status of the alarm (ALARM pin). Additionally, two more digital outputs are prepared to include an electro-mechanical lock system to the door (pins LOCK and UNLOCK); but currently these feature has not been implemented in the firmware.
The digital outputs are conceived to be used with standard relay boards as the one shown in Figure 7. These can be sourced from many internet suppliers for very few euros.
To operate with these relay boards, the following cabling should be performed (Figure 8):
-	Digital outputs driving signals (green and blue traces)
-	Driving signals voltage (yellow trace)
-	Relay coils voltage (red and black traces)

Mechanical interface
The board 1 includes four 2 mm diameter holes to be fixed to a frame by means of plastic spacers. The pattern for the holes is shown in Figure 9.

Board 2
The board 2 comprises the RFID reader and an LCD display that shows information about the status of the device. This board is intended to be used with an ID-12LA reader and a breakout board   (https://www.sparkfun.com/products/13030) but other serial interfaced readers might be used.
This board receives the power supply from the link with the board 1 and converts it to a 5 Vdc stabilized voltage by means of its onboard regulator (U2_1 in Figure 10).

Board 2 also includes a buzzer (SG1 in Figure 10) that gives a sound feedback when a RFID tag has been read. It is additionally used to give some other information to the user such as a counter that is about to finish.
Links with the board 1
The links with the board 1 are highlighted in the next figure with corresponding colors to Figure 5. An important remark is that the pin ID1_TX1 in Figure 11 should be wired to the pin Rx in Figure 5.

Mechanical interface
The board 2 includes four 2 mm diameter holes to be fixed to a frame by means of plastic spacers. The pattern for the holes is shown in Figure 12.

Boards interconnection
The following figure summarizes the cabling between both boards.

