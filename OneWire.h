#pragma once

//#ifdef __cplusplus
#include "defines.h"
__attribute__((weak)) extern const uint8_t pin_onewire;

class OneWire
{
protected:
#if ONEWIRE_SEARCH
	// global search state
	unsigned char ROM_NO[8];
	uint8_t LastDiscrepancy;
	uint8_t LastFamilyDiscrepancy;
	bool LastDeviceFlag;
#endif
public:
	OneWire() { };
	//OneWire(uint8_t pin) { begin(pin); }
	void begin() {
#if not defined __AVR__ && defined OUTPUT_OPEN_DRAIN
		pInit(pin, OUTPUT_OPEN_DRAIN);
		DIRECT_WRITE_HIGH(pin);
#else
	//pInit(pin, INPUT);
#endif // __AVR__
		pInit(pin_onewire, INPUT);
#if ONEWIRE_SEARCH
		reset_search();
#endif
	};

	bool reset(void) {
		bool r;
		if (!DIRECT_READ(pin_onewire)) {
			for (uint8_t retries = 255; !DIRECT_READ(pin_onewire); --retries) {
				if (retries == 0) return false; // wait until the wire is high... just in case
				delayUs(1);
			};
		}
		DIRECT_WRITE_LOW(pin_onewire);	// drive output low
		delayUs(480);
		DIRECT_WRITE_HIGH(pin_onewire);	// allow it to float
		noInterrupts();
		delayUs(69);
		r = !DIRECT_READ(pin_onewire);
		delayUs(230);
		r &= DIRECT_READ(pin_onewire);
		delayUs(180);
		r &= DIRECT_READ(pin_onewire);
		interrupts();
		return r;
	};

	void write_bit(bool v) {
		noInterrupts();
		DIRECT_WRITE_LOW(pin_onewire);	// drive output low
		if (v) {
			delayUs(TIMESLOT_START);
			DIRECT_WRITE_HIGH(pin_onewire);	//// drive output high
			delayUs(TIMESLOT - TIMESLOT_START);
		} else {
			delayUs(TIMESLOT_LOW);
			DIRECT_WRITE_HIGH(pin_onewire);	//// drive output high
			delayUs(TIMESLOT - TIMESLOT_LOW);
		}
		interrupts();
	};

	bool read_bit(void) {
		bool r;
		noInterrupts();
		DIRECT_WRITE_LOW(pin_onewire);
		delayUs(TIMESLOT_START);
		DIRECT_WRITE_HIGH(pin_onewire);	// let pin float, pull up will raise
		delayUs(TIMESLOT_READ);
		r = DIRECT_READ(pin_onewire);
		delayUs(TIMESLOT - TIMESLOT_READ - TIMESLOT_START);
		interrupts();
		return r;
	}
	// Write a byte.
	void write(uint8_t B/*, bool power = 0*/) {
		for (uint8_t mask = 1; mask; mask <<= 1) write_bit(mask & B);
	}

	void write_bytes(const uint8_t* buf, uint16_t count) { 
		for (uint16_t i = 0; i < count; i++) write(buf[i]); 
	}
	// Read a byte.
	uint8_t read(void) {
		for (uint8_t mask = 1, B = 0;;) { if (read_bit()) B |= mask; if ((mask <<= 1) == 0) return B; }
	}

	void read_bytes(uint8_t* buf, uint16_t count) { for (uint16_t i = 0; i < count; i++)  buf[i] = read(); }

	void skip(void) { write(0xCC); };       // Skip ROM

	void select(const uint8_t(&rom)[8]) { write(0x55); write_bytes(rom, 8); };

	bool search(uint8_t(&rom)[8]) {		//simple search with out buffer
		uint8_t i = 0, data, bitmask, bit_id;
		if (!reset()) return false;
		write(0xF0);
		do {
			for (bitmask = 1, data = 0; bitmask; bitmask <<= 1) {
				bit_id = read_bit();
				if (bit_id == read_bit()) return false;
				if (bit_id) data |= bitmask;
				write_bit(bit_id);
			}
			rom[i] = data;
		} while (++i < 8);
		return true;
	};

	virtual void power() {};
	virtual void depower() {};

#if ONEWIRE_SEARCH
	// Clear the search state so that if will start from the beginning again.
	void reset_search() {
	// reset the search state
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		LastFamilyDiscrepancy = 0;
		memset(ROM_NO, 0, 8);
	};
	// Setup the search to find the device type 'family_code' on the next call
	// to search(*newAddr) if it is present.
	void target_search(uint8_t family_code) {
	// set the search state to find SearchFamily type devices
		ROM_NO[0] = family_code;
		memset(&ROM_NO[1], 0, 7);
		LastDiscrepancy = 64;
		LastFamilyDiscrepancy = 0;
		LastDeviceFlag = false;
	};
	// Look for the next device. Returns 1 if a new address has been
	// returned. A zero might mean that the bus is shorted, there are
	// no devices, or you have already retrieved all of them.  It
	// might be a good idea to check the CRC to make sure you didn't
	// get garbage.  The order is deterministic. You will always get
	// the same devices in the same order.
	bool search(uint8_t* newAddr, bool search_mode = true) {
		uint8_t id_bit_number = 1, last_zero = 0, i = 0, rom_mask;
		bool search_result = false, bit_id, bit_inv, direction;
		if (!LastDeviceFlag) { // if the last call was not the last one 
			if (!reset()) { // 1-Wire reset
				reset_search();
				return false;
			}
			if (search_mode) write(0xF0);   // NORMAL SEARCH
			else write(0xEC);   // CONDITIONAL SEARCH
			do {  // loop to do the search
				for (rom_mask = 1; rom_mask; rom_mask <<= 1, id_bit_number++) {
					bit_id = read_bit(); // read a bit and its complement
					bit_inv = read_bit();
					// check for no devices on 1-wire
					if ((bit_id == 1) && (bit_inv == 1)) goto _exit;
					else if (bit_id != bit_inv) direction = bit_id;// all devices coupled have 0 or 1	  // bit write value for search
					else {
						// if this discrepancy if before the Last Discrepancy on a previous next then pick the same as last time
						if (id_bit_number < LastDiscrepancy) direction = ((ROM_NO[i] & rom_mask) > 0);
						else direction = (id_bit_number == LastDiscrepancy); // if equal to last pick 1, if not then pick 0
						if (direction == 0) { // if 0 was picked then record its position in LastZero
							last_zero = id_bit_number;
							if (last_zero < 9)  LastFamilyDiscrepancy = last_zero; // check for Last discrepancy in family
						}
					}
					if (direction == 1) ROM_NO[i] |= rom_mask; // set or clear the bit in the ROM byte
					else ROM_NO[i] &= ~rom_mask;
					write_bit(direction); // serial number search direction write bit
				}
			} while (++i < 8);  // loop until through all ROM bytes 0-7
			// if the search was successful then
			if (id_bit_number > 64) {
				LastDiscrepancy = last_zero;// search successful so set LastDiscrepancy,LastDeviceFlag,search_result 
				if (LastDiscrepancy == 0) LastDeviceFlag = true; // check for last device
				search_result = true;
			}
		}
_exit: // if no device found then reset counters so next 'search' will be like a first
		if (!search_result || !ROM_NO[0]) { reset_search(); } else for (uint8_t i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
		return search_result;
};
#endif

#if ONEWIRE_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
#if ONEWIRE_CRC8_TABLE
// Dow-CRC using polynomial X^8 + X^5 + X^4 + X^0
// Tiny 2x16 entry CRC table created by Arjen Lentz
// See http://lentz.com.au/blog/calculating-crc-with-a-tiny-32-entry-lookup-table
	static const uint8_t PROGMEM dscrc2x16_table[] = {
		0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83,
		0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
		0x00, 0x9D, 0x23, 0xBE, 0x46, 0xDB, 0x65, 0xF8,
		0x8C, 0x11, 0xAF, 0x32, 0xCA, 0x57, 0xE9, 0x74
	};
	// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM and the registers. (Use tiny 2x16 entry CRC table)
	uint8_t OneWire::crc8(const uint8_t* addr, uint8_t len) {
		uint8_t crc = 0;
		while (len--) {
			crc ^= *addr++;  // just re-using crc as intermediate
			crc = pgm_read_byte(dscrc2x16_table + (crc & 0x0f)) xor pgm_read_byte(dscrc2x16_table + 16 + ((crc >> 4) & 0xF));
		}
		return crc;
	}
#else
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but a little smaller, than the lookup table.
	uint8_t crc8(const uint8_t* addr, uint8_t len) {
		uint8_t crc = 0;
		while (len--) {
#if defined(__AVR__)
			crc = _crc_ibutton_update(crc, *addr++);
#else
			uint8_t inbyte = *addr++;
			for (uint8_t i = 8; i; i--) {
				uint8_t mix = (crc ^ inbyte) & 0x01;
				crc >>= 1;
				if (mix) crc ^= 0x8C;
				inbyte >>= 1;
			}
#endif
		}
		return crc;
	}
#endif

#if ONEWIRE_CRC16
	// Compute the 1-Wire CRC16 and compare it against the received CRC.
	// Example usage (reading a DS2408):
	//    // Put everything in a buffer so we can compute the CRC easily.
	//    uint8_t buf[13];
	//    buf[0] = 0xF0;    // Read PIO Registers
	//    buf[1] = 0x88;    // LSB address
	//    buf[2] = 0x00;    // MSB address
	//    WriteBytes(net, buf, 3);    // Write 3 cmd bytes
	//    ReadBytes(net, buf+3, 10);  // Read 6 data bytes, 2 0xFF, 2 CRC16
	//    if (!CheckCRC16(buf, 11, &buf[11])) {
	//        // Handle error.
	//    }     
	//          
	// @param input - Array of bytes to checksum.
	// @param len - How many bytes to use.
	// @param inverted_crc - The two CRC16 bytes in the received data.
	//                       This should just point into the received data,
	//                       *not* at a 16-bit integer.
	// @param crc - The crc starting value (optional)
	// @return True, iff the CRC matches.
	static bool check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc = 0) {
		crc = ~crc16(input, len, crc);
		return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
	};

	// Compute a Dallas Semiconductor 16 bit CRC.  This is required to check
	// the integrity of data received from many 1-Wire devices.  Note that the
	// CRC computed here is *not* what you'll get from the 1-Wire network,
	// for two reasons:
	//   1) The CRC is transmitted bitwise inverted.
	//   2) Depending on the endian-ness of your processor, the binary
	//      representation of the two-byte return value may have a different
	//      byte order than the two bytes you get from 1-Wire.
	// @param input - Array of bytes to checksum.
	// @param len - How many bytes to use.
	// @param crc - The crc starting value (optional)
	// @return The CRC16, as defined by Dallas Semiconductor.
	static uint16_t crc16(const uint8_t* input, uint16_t len, uint16_t crc = 0) {
#if defined(__AVR__)
		for (uint16_t i = 0; i < len; i++) {
			crc = _crc16_update(crc, input[i]);
		}
#else
		static const uint8_t oddparity[16] =
		{ 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

		for (uint16_t i = 0; i < len; i++) {
			// Even though we're just copying a byte from the input,
			// we'll be doing 16-bit computation with it.
			uint16_t cdata = input[i];
			cdata = (cdata ^ crc) & 0xff;
			crc >>= 8;

			if (oddparity[cdata & 0x0F] ^ oddparity[cdata >> 4])
				crc ^= 0xC001;

			cdata <<= 6;
			crc ^= cdata;
			cdata <<= 1;
			crc ^= cdata;
		}
#endif
		return crc;
	};
#endif
#endif
};
// undef defines for no particular reason
#ifdef ARDUINO_ARCH_ESP32
#  undef noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#  undef interrupts() portEXIT_CRITICAL(&mux);}
#endif
// for info on this, search "IRAM_ATTR" at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html 
#undef CRIT_TIMING 
//#endif // __cplusplus
