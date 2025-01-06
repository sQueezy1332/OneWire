#pragma once
#include <stdint.h>
#if defined(__AVR__)
#include <util/crc16.h>
#endif
#include <Arduino.h>       // for delayMicroseconds, digitalPinToBitMask, etc
#include <GyverIO.h>
#define pMode(pin, mode)    pinMode(pin, mode)
// You can exclude certain features from OneWire.  In theory, this
// might save some space.  In practice, the compiler automatically
// removes unused code (technically, the linker, using -fdata-sections
// and -ffunction-sections when compiling, and Wl,--gc-sections
// when linking), so most of these will not result in any code size
// reduction.  Well, unless you try to use the missing features
// and redesign your program to not need them!  ONEWIRE_CRC8_TABLE
// is the exception, because it selects a fast but large algorithm
// or a small but slow algorithm.

// you can exclude onewire_search by defining that to 0
#ifndef ONEWIRE_SEARCH
#define ONEWIRE_SEARCH 0
#endif

// You can exclude CRC checks altogether by defining this to 0
#ifndef ONEWIRE_CRC
#define ONEWIRE_CRC 1
#endif
// Select the table-lookup method of computing the 8-bit CRC
// by setting this to 1.  The lookup table enlarges code size by
// about 250 bytes.  It does NOT consume RAM (but did in very
// old versions of OneWire).  If you disable this, a slower
// but very compact algorithm is used.
#ifndef ONEWIRE_CRC8_TABLE
#define ONEWIRE_CRC8_TABLE 0
#endif

// You can allow 16-bit CRC checks by defining this to 1
// (Note that ONEWIRE_CRC must also be 1.)
#ifndef ONEWIRE_CRC16
#define ONEWIRE_CRC16 0
#endif
#define TIMESLOT 60
#define TIMESLOT_START 3

#define pInit(pin, mode)    gio::init(pin, mode)
#define DIRECT_READ(pin)	gio::read(pin)
#if not defined __AVR__ && defined OUTPUT_OPEN_DRAIN
#define DIRECT_WRITE_HIGH(pin)	gio::high(pin)	//OPEN_DRAIN
#define DIRECT_WRITE_LOW(pin) gio::low(pin)
#else
#define DIRECT_WRITE_HIGH(pin)	gio::mode(pin, INPUT)
#define DIRECT_WRITE_LOW(pin) gio::mode(pin, OUTPUT)
#endif
#define systime micros()
#define mS millis()
#define delayUs(us) delayMicroseconds(us)
#define delayMs(us) delay(us)

#ifdef ARDUINO_ARCH_ESP32
// for info on this, search "IRAM_ATTR" at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html 
#define CRIT_TIMING IRAM_ATTR
#define noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#define interrupts() portEXIT_CRITICAL(&mux);}
#else
#define CRIT_TIMING 
#endif
#define UNUSED __attribute__((unused))
