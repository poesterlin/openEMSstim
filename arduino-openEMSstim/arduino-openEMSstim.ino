/**
 * arduino-openEMSstim: https://github.com/PedroLopes/openEMSstim
 * a mod of the original [1] by Pedro Lopes, see 
 * [1] <https://bitbucket.org/MaxPfeiffer/letyourbodymove/wiki/Home/License>
 * @license "The MIT License (MIT) – military use of this product is forbidden – V 0.2"
 */ 

// Necessary files (AltSoftSerial.h, AD5252.h, Rn4020BTLe.h, EMSSystem.h, EMSChannel.h) and dependencies (Wire.h, Arduino.h)
//#include "Arduino_Software.h"
#include "Arduino.h"
#include "AltSoftSerial.h"
#include "Wire.h"
#include "AD5252.h"
#include "Rn4020BTLe.h"
#include "EMSSystem.h"
#include "EMSChannel.h"
#include "EMSUtil.h"
#include "avr/pgmspace.h"
#include "avr/wdt.h"

//BT: the string below is how your EMS module will show up for other BLE devices
#define EMS_BLUETOOTH_ID "TEST1"

//DEBUG: setup for verbose mode (prints debug messages if DEBUG_ON is 1)
#define DEBUG_ON 1

//USB: allows commands using the full protocol (refer to https://github.com/PedroLopes/openEMSstim) (by default this is active)
#define USB_FULL_COMMANDS_ACTIVE 1 

//USB: allows to send simplified test commands (one char each, refer to https://github.com/PedroLopes/openEMSstim) to the board via USB (by default this is inactive)
#define USB_TEST_COMMANDS_ACTIVE 0

//SAFETY: resets the board (which opens all channels) if the main loop hangs for 2 s, e.g. on a stuck I2C bus.
//Needs a bootloader that clears the watchdog after a reset (Optiboot: Uno, Nano "new bootloader").
//Set to 0 if your Nano uses the old bootloader, otherwise it will get stuck in a reset loop.
#define WATCHDOG_ACTIVE 1

//helper print function that handles the DEBUG_ON flag automatically
void printer(String msg, boolean force = false) {
  if (DEBUG_ON || force) {
    Serial.println(msg);
  }
}

//Initialization of control objects
AltSoftSerial softSerial;
AD5252 digitalPot(0);
Rn4020BTLe bluetoothModule(2, &softSerial);
EMSChannel emsChannel1(5, 4, A2, &digitalPot, 1);
EMSChannel emsChannel2(6, 7, A3, &digitalPot, 3);
EMSSystem emsSystem(2);

void setup() {
	MCUSR = 0;
	wdt_disable();
	Serial.begin(19200);
	softSerial.setTimeout(100);
	Serial.setTimeout(50);
	printer("\nSETUP:");
	Serial.flush();

	//Reset and Initialize the Bluetooth module
	printer("\tBT: RESETTING");
	bluetoothModule.reset();
	printer("\tBT: RESET DONE");
	printer("\tBT: INITIALIZING");
	bluetoothModule.init(EMS_BLUETOOTH_ID);
	printer("\tBT: INITIALIZED");

	//Add the EMS channels and start the control
	printer("\tEMS: INITIALIZING CHANNELS");
	emsSystem.addChannelToSystem(&emsChannel1);
	emsSystem.addChannelToSystem(&emsChannel2);
	EMSSystem::start();
	Wire.setWireTimeout(25000, true); //never block forever on a stuck I2C bus (needs Arduino AVR core >= 1.8.3)
	//the AD5252 restores its last wiper position on power-up: start at minimum intensity
	if (!digitalPot.setPosition(1, POTI_STEPS_UP) || !digitalPot.setPosition(3, POTI_STEPS_UP)) {
		printer("\tEMS: ERROR, could not reset the digital potentiometer", true);
	}
	printer("\tEMS: INITIALIZED");
	printer("\tEMS: STARTED");
	printer("SETUP DONE (LED 13 WILL BE ON)");
	pinMode(13, OUTPUT);
	digitalWrite(13, HIGH);
#if WATCHDOG_ACTIVE
	wdt_enable(WDTO_2S);
#endif
}

String command = "";
String hexCommandString;
const String BTLE_DISCONNECT = "Connection End";

void loop() {
	wdt_reset();

	if (softSerial.available() > 0) {
		String message = softSerial.readStringUntil('\n');
		message.trim();
                printer("\tBT: received command: " + String(message));
		processMessage(message, false, false);
                softSerial.flush(); //never tested
	}

	//Checks whether a signal has to be stoped
	if (emsSystem.check() > 0) {

	}

	//Communicate to the EMS-module over USB
        if (Serial.available() > 0) {
           if (USB_FULL_COMMANDS_ACTIVE) {
             String message = Serial.readStringUntil('\n');
             printer("\tUSB: received command: " + String(message));
             message.trim();   
	     processMessage(message, USB_TEST_COMMANDS_ACTIVE, true);
           } else if (USB_TEST_COMMANDS_ACTIVE) {
             char c = Serial.read();
             printer("\tUSB-TEST-MODE: received command: " + char(c));
	     doCommand(c);
	   }
          Serial.flush(); 
	}
}

const char ems_channel_1_active[] PROGMEM =    "\tEMS: Channel 1 active";
const char ems_channel_1_inactive[]  PROGMEM = "\tEMS: Channel 1 inactive";
const char ems_channel_2_active[] PROGMEM =    "\tEMS: Channel 2 active";
const char ems_channel_2_inactive[] PROGMEM =  "\tEMS: Channel 2 inactive";
const char ems_channel_1_intensity[] PROGMEM = "\tEMS: Intensity Channel 1: ";
const char ems_channel_2_intensity[] PROGMEM = "\tEMS: Intensity Channel 2: ";

const char* const string_table_outputs[] PROGMEM = {ems_channel_1_active, ems_channel_1_inactive, ems_channel_2_active, ems_channel_2_inactive, ems_channel_1_intensity, ems_channel_2_intensity};

char buffer[32];

//process a command message (according to protocol, check github for that)
//  allowTestCommands: accept single-char test commands (see below)
//  allowRaw: accept plain protocol commands such as "C0I100T1000G" (USB only;
//            on BT, any line that is not "WV,..." is a status message of the module)
void processMessage(String message, bool allowTestCommands, bool allowRaw) {
  if (message.equals(BTLE_DISCONNECT)) {
    printer("\tBT: Disconnected");
    emsSystem.shutDown();
    return;
  }
  switch (ems::classifyMessage(message.c_str(), message.length())) {
  case ems::MESSAGE_HEX: {
    //"WV,<handle>,<hex>." -> hex is between the last comma and the final '.'
    int lastIndexOfComma = message.lastIndexOf(',');
    hexCommandString = message.substring(lastIndexOfComma + 1,
    message.length() - 1);
    printer("\tEMS_CMD: HEX command length: ");
    printer(String(hexCommandString.length()));
    printer(hexCommandString);
    char decoded[32];
    if (lastIndexOfComma == -1 || !ems::decodeHex(hexCommandString.c_str(),
        hexCommandString.length(), decoded, sizeof(decoded))) {
      printer("\tERROR: malformed HEX command, ignored", true);
      return;
    }
    command = decoded;
    printer("\tEMS_CMD: Converted HEX command: ");
    printer(command);
    emsSystem.doCommand(&command);
    break;
  }
  case ems::MESSAGE_TEST_CHAR:
    if (allowTestCommands) {
      doCommand(message[0]);
    } else {
      printer("\tERROR: test commands are disabled (USB_TEST_COMMANDS_ACTIVE)");
    }
    break;
  case ems::MESSAGE_RAW:
    if (allowRaw) {
      command = message;
      emsSystem.doCommand(&command);
    } else {
      printer("\tCommand NON HEX:");
      printer(message);
    }
    break;
  default:
    break;
  }
}



// TESTING COMMANDS
//   For quick testing (e.g., opening the optocoupler ports, etc) you can use these single-char commands (one command = one char).
//   The cavailable commands are:
//   An opened channel closes by itself after EMS_MAX_SIGNAL_LENGTH_MS, and the intensity never wraps around (255 + 1 stays 255).
//     "1": toggles the channel 1 between open/closed state (when the channel is closed, the potenciometer position is reset to minimum, i.e., 255)
//     "2": toggles the channel 1 between open/closed state (when the channel is closed, the potenciometer position is reset to minimum, i.e., 255)
//     "q": increases EMS signal on channel 1 by decreasing the digital potenciometer wiper position (i.e., resistance lowers and more EMS is passing)
//           -> note that you have full EMS signal when the potentiometer wiper is at 0 (0 is the maximum) 
//     "a": decreases EMS signal on channel 1 by increasing the digital potenciometer wiper position (i.e., resistance lowers and more EMS is passing)
//           -> note that you have no EMS signal when the potentiometer wiper is at 255 (255 is the maximum resistance of the potentiometer) 
//     "w": increases EMS signal on channel 2 by decreasing the digital potenciometer wiper position (i.e., resistance lowers and more EMS is passing)
//           -> note that you have full EMS signal when the potentiometer wiper is at 0 (0 is the maximum) 
//     "s": decreases EMS signal on channel 2 by increasing the digital potenciometer wiper position (i.e., resistance lowers and more EMS is passing)
//           -> note that you have no EMS signal when the potentiometer wiper is at 255 (255 is the maximum resistance of the potentiometer) 
void doCommand(char c) {
      if (c == '1') {
		if (emsChannel1.isActivated()) {
			emsChannel1.deactivate();
      strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[1])));
			printer(buffer); //"\tEMS: Channel 1 inactive"
		} else {
			emsChannel1.setSignalLength(EMS_MAX_SIGNAL_LENGTH_MS);
			emsChannel1.activate();
			emsChannel1.applySignal();
      strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[0])));
			printer(buffer); //"\tEMS: Channel 1x active"
		}
	} else if (c == '2') {
		if (emsChannel2.isActivated()) {
			emsChannel2.deactivate();
      strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[3])));
			printer(buffer); //"\tEMS: Channel 2 inactive"
		} else {
			emsChannel2.setSignalLength(EMS_MAX_SIGNAL_LENGTH_MS);
			emsChannel2.activate();
			emsChannel2.applySignal();
      strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[2])));
			printer(buffer);  //"\tEMS: Channel 2 inactive"
		}
	} else if (c == 'a') {
		stepTestWiper(1, +1);
    strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[4])));
		printer(
				buffer + String(digitalPot.getPosition(1))); //"\tEMS: Intensity Channel 1: "
	} else if (c == 'q') {
    strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[4])));
		stepTestWiper(1, -1);
		printer(
				buffer + String(digitalPot.getPosition(1))); //"\tEMS: Intensity Channel 1: "
	} else if (c == 's') {
		//Note that this is channel 3 on Digipot but EMS channel 2
		stepTestWiper(3, +1);
   strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[5])));
		printer(
				buffer + String(digitalPot.getPosition(3))); //"\tEMS: Intensity Channel 2: "
	} else if (c == 'w') {
		//Note that this is channel 3 on Digipot but EMS channel 2
		stepTestWiper(3, -1);
   strcpy_P(buffer, (char*)pgm_read_word(&(string_table_outputs[5])));
		printer(
				buffer + String(digitalPot.getPosition(3))); //"\tEMS: Intensity Channel 2: "
	}
        else printer("\tERROR: SINGLE-CHAR Command Unknown");
}

//moves a wiper by one step without wrapping; on an I2C error both channels are closed
void stepTestWiper(uint8_t wiperIndex, int delta) {
	int position = digitalPot.getPosition(wiperIndex);
	if (position < 0 || !digitalPot.setPosition(wiperIndex, ems::stepWiper(position, delta))) {
		printer("\tERROR: could not set intensity (I2C), closing channels", true);
		emsSystem.shutDown();
	}
}
