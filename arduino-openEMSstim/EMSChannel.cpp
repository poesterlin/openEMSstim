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
 * EMSChannel.cpp
 *
 *  Created on: 06.05.2014
 *  Author: Tim Duente
 *  Edit by: Max Pfeiffer 15.06.2015
 */
#include "EMSChannel.h"

//---------- constructor ----------------------------------------------------

EMSChannel::EMSChannel(uint8_t channel_to_Pads, uint8_t channel_to_Pads_2,
		uint8_t led_active_pin, AD5252* digitalPoti, uint8_t wiperIndex) {
	intensity = 0;
	activated = false;

	maxIntensity = 215; // Die differnez ist gleich dem max Wert
	minIntensity = 55; //

	this->channel_to_Pads = channel_to_Pads;
	this->channel_to_Pads_2 = channel_to_Pads_2;
	this->led_active_pin = led_active_pin;

	endTime = 0;
	timerArmed = false;
	onTimeChannel = 1;

	this->digitalPoti = digitalPoti;
	this->wiperIndex = wiperIndex;

	pinMode(channel_to_Pads, OUTPUT);
	pinMode(channel_to_Pads_2, OUTPUT);
	pinMode(led_active_pin, OUTPUT);

	digitalWrite(channel_to_Pads_2, LOW);
	digitalWrite(channel_to_Pads, LOW);
	digitalWrite(led_active_pin, LOW);
}

EMSChannel::~EMSChannel() {
}

//---------- public ----------------------------------------------------
/*
 * Starts the communication with the digital Poti. Must be called for initialization
 */
void EMSChannel::start() {
	Wire.begin();
}

/*
 * Schaltet das EMS-Signal auf die Pads.
 */
void EMSChannel::activate() {
	digitalWrite(channel_to_Pads, HIGH);
	digitalWrite(channel_to_Pads_2, HIGH);
	activated = true;
	digitalWrite(led_active_pin, HIGH);
}
/*
 * Schaltet das EMS-Signal auf den MosFet.
 */
void EMSChannel::deactivate() {
	digitalPoti->setPosition(wiperIndex, 255);
	delay(50);
	digitalWrite(led_active_pin, LOW);
	digitalWrite(channel_to_Pads_2, LOW);
	digitalWrite(channel_to_Pads, LOW);
	activated = false;
	endTime = 0;
	timerArmed = false;
}

/* EN: Proofs if the channel is active
 */
bool EMSChannel::isActivated() {
	return activated;
}

/* Sets the intensity from 0-100 (out-of-range values are clamped, so the
 * calibrated maximum is never exceeded). Returns false if the digital poti
 * could not be written; the channel must not be activated in that case.
 */
bool EMSChannel::setIntensity(int intensity) {
	this->intensity = ems::intensityToLevel(intensity, minIntensity,
			maxIntensity);

	int resistorLevel = POTI_STEPS_UP - this->intensity;

	return digitalPoti->setPosition(wiperIndex, resistorLevel);
}

/* Writes the last intensity set with setIntensity() to the poti again (it is
 * reset to minimum whenever the channel is deactivated).
 */
bool EMSChannel::restoreIntensity() {
	return digitalPoti->setPosition(wiperIndex, POTI_STEPS_UP - intensity);
}

/* Return the intensity in a range of 0-100

 */
int EMSChannel::getIntensity() {
	return intensity;
}

void EMSChannel::setSignalLength(int signalLength) {
	onTimeChannel = (int) ems::clamp(signalLength, 0, EMS_MAX_SIGNAL_LENGTH_MS);

}

int EMSChannel::getSignalLength() {
	return onTimeChannel;

}

void EMSChannel::applySignal() {
	endTime = millis() + onTimeChannel;
	timerArmed = true;
}


int EMSChannel::check() {
	if (timerArmed && ems::deadlineReached(millis(), endTime)) {
		deactivate();
		return 1;
	}
	return 0;
}

//maxIntensity in percent
void EMSChannel::setMaxIntensity(int maxIntensity) {
	this->maxIntensity = ems::percentToLevel(maxIntensity);
}

//minIntensity in percent
void EMSChannel::setMinIntensity(int minIntensity) {
	this->minIntensity = ems::percentToLevel(minIntensity);
}

//---------- private ----------------------------------------------------
