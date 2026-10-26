/**
 * arduino-openEMSstim: https://github.com/PedroLopes/openEMSstim
 * @license "The MIT License (MIT) – military use of this product is forbidden – V 0.2"
 */

/*
 * EMSUtil.h
 *
 * Pure helpers (no Arduino dependencies) so they can be unit tested on a host
 * machine, see tests/firmware.
 */

#ifndef EMSUTIL_H_
#define EMSUTIL_H_

#include <stddef.h>
#include <stdint.h>

// Longest on-time a single command (or test toggle) may keep a channel open.
#define EMS_MAX_SIGNAL_LENGTH_MS 5000

namespace ems {

// Returns 0-15 for a hex digit (either case), -1 otherwise.
inline int hexNibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

// Decodes hex pairs into out (NUL-terminated). Returns false on odd length,
// invalid digits, a decoded NUL byte or if out is too small.
inline bool decodeHex(const char *hex, size_t len, char *out, size_t outSize) {
	if (len % 2 != 0 || len / 2 + 1 > outSize) return false;
	for (size_t i = 0; i < len; i += 2) {
		int high = hexNibble(hex[i]);
		int low = hexNibble(hex[i + 1]);
		if (high < 0 || low < 0 || (high == 0 && low == 0)) return false;
		out[i / 2] = (char) (high * 16 + low);
	}
	out[len / 2] = '\0';
	return true;
}

inline long clamp(long value, long low, long high) {
	if (value < low) return low;
	if (value > high) return high;
	return value;
}

// Maps an intensity of 0-100 % onto the calibrated level range [minLevel,
// maxLevel] (0-255, higher = stronger). Out-of-range input is clamped, so the
// calibrated maximum can never be exceeded.
inline uint8_t intensityToLevel(long percent, int minLevel, int maxLevel) {
	long low = clamp(minLevel, 0, 255);
	long high = clamp(maxLevel, low, 255);
	percent = clamp(percent, 0, 100);
	return (uint8_t) (low + ((high - low) * percent + 50) / 100);
}

// Converts a calibration percentage (0-100) into a level (0-255).
inline int percentToLevel(long percent) {
	return (int) ((clamp(percent, 0, 100) * 255 + 50) / 100);
}

// Moves a digital potentiometer wiper without wrapping around at 0 / 255.
inline uint8_t stepWiper(uint8_t position, int delta) {
	return (uint8_t) clamp((long) position + delta, 0, 255);
}

// True once now has reached deadline; correct across millis() overflow.
inline bool deadlineReached(unsigned long now, unsigned long deadline) {
	return (long) (now - deadline) >= 0;
}

enum MessageKind {
	MESSAGE_EMPTY,
	MESSAGE_HEX,       // "WV,<handle>,<hex>." written by the RN4020 BLE module
	MESSAGE_TEST_CHAR, // single-char test command, e.g. "1" or "q"
	MESSAGE_RAW        // plain protocol command, e.g. "C0I100T1000G"
};

inline MessageKind classifyMessage(const char *message, size_t len) {
	if (len == 0) return MESSAGE_EMPTY;
	if (len >= 2 && message[0] == 'W' && message[1] == 'V') return MESSAGE_HEX;
	if (len == 1) return MESSAGE_TEST_CHAR;
	return MESSAGE_RAW;
}

} // namespace ems

#endif /* EMSUTIL_H_ */
