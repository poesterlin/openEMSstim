// Host-side stand-in for the Arduino Wire (I2C) library that models an AD5252.
#ifndef MOCK_WIRE_H_
#define MOCK_WIRE_H_

#include <cstdint>

struct MockWire {
	void begin() {}
	void beginTransmission(uint8_t) { count = 0; }
	void write(uint8_t b) { buf[count < 2 ? count : 1] = b; count++; }
	uint8_t endTransmission(uint8_t = 1) {
		if (failWrites) return 2; // NACK on address
		if (count >= 1) reg = buf[0];
		if (count == 2) wiper[buf[0] & 3] = buf[1];
		return 0;
	}
	uint8_t requestFrom(uint8_t, uint8_t n) { return failWrites ? 0 : n; }
	int read() { return wiper[reg & 3]; }

	bool failWrites = false;
	uint8_t wiper[4] = {128, 128, 128, 128}; // AD5252 power-on midscale
	uint8_t reg = 0, buf[2] = {0, 0};
	int count = 0;
};
extern MockWire Wire;

#endif
