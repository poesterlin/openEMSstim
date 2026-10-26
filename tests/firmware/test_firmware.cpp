// Host-side tests for the openEMSstim firmware logic. Run ./run.sh
#include <cstdio>
#include <cstring>
#include "Arduino.h"
#include "Wire.h"
#include "EMSUtil.h"
#include "AD5252.h"
#include "EMSChannel.h"
#include "EMSSystem.h"

unsigned long mock::now = 0;
int mock::pins[64] = {0};
MockSerial Serial;
MockWire Wire;

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { checks++; long _a = (long) (a), _b = (long) (b); if (_a != _b) { failures++; std::printf("FAIL %s:%d: %s == %s (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

// Same wiring as arduino-openEMSstim.ino
struct Board {
	AD5252 pot{0};
	EMSChannel ch1{5, 4, 16, &pot, 1};
	EMSChannel ch2{6, 7, 17, &pot, 3};
	EMSSystem sys{2};
	Board() {
		Wire = MockWire();
		mock::now = 1000;
		sys.addChannelToSystem(&ch1);
		sys.addChannelToSystem(&ch2);
	}
	void send(const char *cmd) { String s(cmd); sys.doCommand(&s); }
	void advance(unsigned long ms) { mock::now += ms; sys.check(); }
};

static void testHexDecoding() {
	char out[32];
	CHECK(ems::decodeHex("433049313030543130303047", 24, out, sizeof(out)));
	CHECK(std::strcmp(out, "C0I100T1000G") == 0);
	// letters A-F used to decode as 0-5, which turned "OMA[0,80]" into "ECAQ0\"80S"
	CHECK(ems::decodeHex("4F4D415B302C38305D", 18, out, sizeof(out)));
	CHECK(std::strcmp(out, "OMA[0,80]") == 0);
	CHECK(ems::decodeHex("4f4d", 4, out, sizeof(out)) && std::strcmp(out, "OM") == 0);
	CHECK(!ems::decodeHex("433", 3, out, sizeof(out)));   // odd length
	CHECK(!ems::decodeHex("4G", 2, out, sizeof(out)));    // invalid digit
	CHECK(!ems::decodeHex("4300", 4, out, sizeof(out)));  // embedded NUL
	CHECK(!ems::decodeHex("434343", 6, out, 3));          // does not fit
}

static void testPureHelpers() {
	CHECK_EQ(ems::intensityToLevel(0, 55, 215), 55);
	CHECK_EQ(ems::intensityToLevel(50, 55, 215), 135);
	CHECK_EQ(ems::intensityToLevel(100, 55, 215), 215);
	// used to reach the full wiper (I151-I205) or overflow the 16-bit int (I206+)
	CHECK_EQ(ems::intensityToLevel(150, 55, 215), 215);
	CHECK_EQ(ems::intensityToLevel(206, 55, 215), 215);
	CHECK_EQ(ems::intensityToLevel(32767, 55, 215), 215);
	CHECK_EQ(ems::intensityToLevel(-5, 55, 215), 55);
	CHECK_EQ(ems::intensityToLevel(100, 200, 100), 200); // max below min
	CHECK_EQ(ems::percentToLevel(80), 204);
	CHECK_EQ(ems::percentToLevel(400), 255);
	CHECK_EQ(ems::stepWiper(255, +1), 255); // used to wrap to 0 = full intensity
	CHECK_EQ(ems::stepWiper(0, -1), 0);
	CHECK_EQ(ems::stepWiper(100, +1), 101);
	unsigned long start = 0xFFFFFF00UL;
	CHECK(!ems::deadlineReached(start, start + 1000));
	CHECK(!ems::deadlineReached(start + 999, start + 1000));
	CHECK(ems::deadlineReached(start + 1000, start + 1000));
	CHECK_EQ(ems::classifyMessage("WV,0018,43.", 11), ems::MESSAGE_HEX);
	CHECK_EQ(ems::classifyMessage("1", 1), ems::MESSAGE_TEST_CHAR);
	CHECK_EQ(ems::classifyMessage("C0I100T1000G", 12), ems::MESSAGE_RAW);
	CHECK_EQ(ems::classifyMessage("", 0), ems::MESSAGE_EMPTY);
}

static void testActionAndTimer() {
	Board b;
	b.send("C0I100T1000G");
	CHECK(b.ch1.isActivated());
	CHECK_EQ(mock::pins[5], HIGH);
	CHECK_EQ(mock::pins[4], HIGH);
	CHECK_EQ(Wire.wiper[1], 255 - 215); // I100 = calibrated maximum, not 99 %
	CHECK(!b.ch2.isActivated());
	b.advance(999);
	CHECK(b.ch1.isActivated());
	b.advance(1);
	CHECK(!b.ch1.isActivated());
	CHECK_EQ(mock::pins[5], LOW);
	CHECK_EQ(Wire.wiper[1], 255);
}

static void testIntensityClamp() {
	Board b;
	b.send("C0I150T100G");
	CHECK_EQ(Wire.wiper[1], 255 - 215);
	b.send("C0I206T100G");
	CHECK_EQ(Wire.wiper[1], 255 - 215);
	b.send("C0I99999T100G");
	CHECK_EQ(Wire.wiper[1], 255 - 215);
	b.send("C0I1234567T100G"); // too many digits: rejected, everything off
	CHECK(!b.ch1.isActivated());
}

static void testCalibrationOption() {
	Board b;
	b.send("OMA[0,80]");
	b.send("C0I100T100G");
	CHECK_EQ(Wire.wiper[1], 255 - 204);
	b.send("OMA[0,900]");
	b.send("C0I100T100G");
	CHECK_EQ(Wire.wiper[1], 0);
}

static void testInvalidChannel() {
	Board b;
	b.send("T100I50G"); // no channel yet: used to dereference emsChannels[-1]
	CHECK(!b.ch1.isActivated() && !b.ch2.isActivated());
	b.send("C5I100T100G"); // out of range: used to write past the array
	CHECK(!b.ch1.isActivated() && !b.ch2.isActivated());
	b.send("C0IT100G"); // intensity without digits
	CHECK(!b.ch1.isActivated());
}

static void testRepeatLastChannel() {
	Board b;
	b.send("C1I50T100G");
	CHECK(b.ch2.isActivated());
	b.advance(100);
	CHECK(!b.ch2.isActivated());
	b.send("G"); // documented: resending "G" repeats the previous command
	CHECK(b.ch2.isActivated());
	CHECK_EQ(Wire.wiper[3], 255 - 135);
}

static void testSignalLengthCap() {
	Board b;
	b.send("C0T60000I10G");
	CHECK_EQ(b.ch1.getSignalLength(), EMS_MAX_SIGNAL_LENGTH_MS);
	b.advance(EMS_MAX_SIGNAL_LENGTH_MS - 1);
	CHECK(b.ch1.isActivated());
	b.advance(1);
	CHECK(!b.ch1.isActivated());
}

static void testI2cFailure() {
	Board b;
	Wire.failWrites = true;
	b.send("C0I100T100G");
	CHECK(!b.ch1.isActivated()); // never open the relays at an unknown intensity
	CHECK_EQ(mock::pins[5], LOW);
	CHECK_EQ(b.pot.getPosition(1), -1);
}

static void testMillisOverflow() {
	Board b;
	mock::now = 0xFFFFFF00UL;
	b.send("C0I10T1000G");
	b.advance(999);
	CHECK(b.ch1.isActivated());
	b.advance(1);
	CHECK(!b.ch1.isActivated());
}

int main() {
	testHexDecoding();
	testPureHelpers();
	testActionAndTimer();
	testIntensityClamp();
	testCalibrationOption();
	testInvalidChannel();
	testRepeatLastChannel();
	testSignalLengthCap();
	testI2cFailure();
	testMillisOverflow();
	std::printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
