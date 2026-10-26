/**
 * ArduinoSoftware_Arduino_IDE
 *
 *  Copyright 2016 by Tim Dünte <tim.duente@hci.uni-hannover.de>
 *  Copyright 2016 by Max Pfeiffer <max.pfeiffer@hci.uni-hannover.de>
 *
 *  Licensed under "The MIT License (MIT) – military use of this product is forbidden – V 0.2".
 *  Some rights reserved. See LICENSE.
 *
 * @license "The MIT License (MIT) – military use of this product is forbidden – V 0.2"
 * <https://bitbucket.org/MaxPfeiffer/letyourbodymove/wiki/Home/License>
 */

/*
 * EMSSystem.cpp
 *
 *  Created on: 26.05.2014
 *  Author: Tim Duente
 *  Edit by: Max Pfeiffer - 13.06.2015
 */

#include "EMSSystem.h"

EMSSystem::EMSSystem(int channels) {
	emsChannels = (EMSChannel**) malloc(channels * sizeof(EMSChannel*));
	channelCount = channels;
	size = 0;
	lastChannel = -1;
}

EMSSystem::~EMSSystem() {
	free(emsChannels);
}

void EMSSystem::addChannelToSystem(EMSChannel *emsChannel) {
	if (size < channelCount) {
		emsChannels[size] = emsChannel;
		size++;
	}
}

// get the next number out of a String object and return it (-1 if there are
// no digits after startIndex or the number is too long to be valid)
long EMSSystem::getNextNumberOfSting(String *command, int startIndex) {
	int endIndex = startIndex + 1;
	while (endIndex < (int) command->length() && endIndex - startIndex <= 6) {
		char tmp = command->charAt(endIndex);
		if (tmp < '0' || tmp > '9') {
			break;
		}
		endIndex++;
	}

	int digits = endIndex - startIndex - 1;
	if (digits < 1 || digits > 5) {
		return -1;
	}
	return command->substring(startIndex + 1, endIndex).toInt();
}

void EMSSystem::doActionCommand(String *command) {

	if (command->length() == 0) {
		Serial.print("Command = null!!!:");
		return;
	}

	// Channel (if omitted, the channel of the previous command is reused)
	int currentChannel = lastChannel;
	int seperatorChannel = command->indexOf(CHANNEL);
	if (seperatorChannel != -1) {
		currentChannel = (int) getNextNumberOfSting(command, seperatorChannel);
	}

	//deactivate all channels if channelNumber is wrong (validated before use)
	if (!isInRange(currentChannel)) {
		shutDown();
		return;
	}
	lastChannel = currentChannel;
	EMSChannel *channel = emsChannels[currentChannel];

	// Signal length onTime (capped at EMS_MAX_SIGNAL_LENGTH_MS)
	int seperatorSignalLength = command->indexOf(TIME);
	if (seperatorSignalLength != -1) {
		long signalLength = getNextNumberOfSting(command, seperatorSignalLength);
		if (signalLength < 0) {
			shutDown();
			return;
		}
		channel->setSignalLength((int) ems::clamp(signalLength, 0,
				EMS_MAX_SIGNAL_LENGTH_MS));
	}

	// Signal Intensity (0-100, clamped; if omitted, the previous one is reused)
	int seperatorSignalIntensity = command->indexOf(INTENSITY);
	bool intensityWritten;
	if (seperatorSignalIntensity != -1) {
		long signalIntensity = getNextNumberOfSting(command,
				seperatorSignalIntensity);
		if (signalIntensity < 0) {
			shutDown();
			return;
		}
		intensityWritten = channel->setIntensity(
				(int) ems::clamp(signalIntensity, 0, 100));
	} else {
		intensityWritten = channel->restoreIntensity();
	}
	if (!intensityWritten) {
		//never open the channel at an unknown intensity
		Serial.println("ERROR: could not set intensity (I2C)");
		channel->deactivate();
		return;
	}

	// Apply the command
	if (command->indexOf(ACTION) != -1) {
		channel->activate();
		channel->applySignal();
	}
}

void EMSSystem::shutDown() {
	for (int i = 0; i < size; i++) {
		emsChannels[i]->deactivate();
	}
}

/* TODO change to set commands */

void EMSSystem::setOption(String *option) {
	char secChar = option->charAt(2);
	int channel = -1;
	int value = -1;
	switch (option->charAt(1)) {
	case 'C':
		if (secChar == 'T' && getChannelAndValue(option, &channel, &value)) {
			//set changeTime
			//emsChannels[channel]->setIncreaseDecreaseTime(value);
		}
		break;
	case 'M':
		if (secChar == 'A' && getChannelAndValue(option, &channel, &value)) {
			//Maxixum value for the calibration
			emsChannels[channel]->setMaxIntensity(value);
		} else if (secChar == 'I'
				&& getChannelAndValue(option, &channel, &value)) {
			//Minimum value for the calibration
			emsChannels[channel]->setMinIntensity(value);
		}
		break;

	default:
		break;
	}

}

bool EMSSystem::getChannelAndValue(String *option, int *channel, int *value) {
	int left = option->indexOf('[');
	int right = option->lastIndexOf(']');
	int seperator = option->indexOf(',', left + 1);

	if (left < seperator && seperator < right && left != -1 && right != -1
			&& seperator != -1) {
		String help = option->substring(left + 1, seperator);
		(*channel) = help.toInt();
		help = option->substring(seperator + 1, right);
		(*value) = help.toInt();

		//Parsing successful
		//Check whether channel exists
		return isInRange((*channel));
	}
	//Parsing not successful
	return false;
}

bool EMSSystem::isInRange(int channel) {
	return (channel >= 0 && channel < size);
}

int EMSSystem::check() {
	int stopCount = 0;
	for (int i = 0; i < size; i++) {
		stopCount = stopCount + emsChannels[i]->check();
	}
	return stopCount;
}

void EMSSystem::doCommand(String *command) {
	if (command->length() > 0) {
		if (command->indexOf(ACTION) != -1) {
			doActionCommand(command);
		} else if (command->charAt(0) == OPTION) {
			setOption(command);
		} else {
			Serial.print("Unknown command: ");
			Serial.println((*command));
			Serial.flush();
		}
	}
}

void EMSSystem::start() {
	EMSChannel::start();
}
