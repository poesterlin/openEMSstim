// Minimal host-side stand-in for the Arduino core, just enough to compile and
// test EMSSystem, EMSChannel and AD5252 on a PC.
#ifndef MOCK_ARDUINO_H_
#define MOCK_ARDUINO_H_

#include <cstdint>
#include <cstdlib>
#include <string>
#include <iostream>

typedef bool boolean;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define B0101100 44

namespace mock {
extern unsigned long now;
extern int pins[64];
}

inline unsigned long millis() { return mock::now; }
inline void delay(unsigned long ms) { mock::now += ms; }
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t value) { mock::pins[pin] = value; }

class String {
public:
	String(const char *s = "") : s_(s) {}
	String(const std::string &s) : s_(s) {}
	String(int v) : s_(std::to_string(v)) {}
	String(long v) : s_(std::to_string(v)) {}
	unsigned int length() const { return s_.size(); }
	char charAt(unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
	char operator[](unsigned int i) const { return charAt(i); }
	const char *c_str() const { return s_.c_str(); }
	int indexOf(char c, unsigned int from = 0) const { size_t p = s_.find(c, from); return p == std::string::npos ? -1 : (int) p; }
	int lastIndexOf(char c) const { size_t p = s_.rfind(c); return p == std::string::npos ? -1 : (int) p; }
	String substring(unsigned int a) const { return a < s_.size() ? String(s_.substr(a)) : String(); }
	String substring(unsigned int a, unsigned int b) const { if (a > b) std::swap(a, b); if (a >= s_.size()) return String(); return String(s_.substr(a, b - a)); }
	long toInt() const { return std::atol(s_.c_str()); }
	bool equals(const String &o) const { return s_ == o.s_; }
	String operator+(const String &o) const { return String(s_ + o.s_); }
	friend std::ostream &operator<<(std::ostream &os, const String &s) { return os << s.s_; }
private:
	std::string s_;
};

struct MockSerial {
	template<typename T> void print(const T &v) { if (verbose) std::cout << v; }
	template<typename T> void println(const T &v) { if (verbose) std::cout << v << "\n"; }
	void flush() {}
	bool verbose = false;
};
extern MockSerial Serial;

#endif
