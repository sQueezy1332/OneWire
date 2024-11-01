#include <Arduino.h>
#include "OneWire.h"
#include "util/OneWire_direct_gpio.h"

#ifdef ARDUINO_ARCH_ESP32
// for info on this, search "IRAM_ATTR" at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html 
#define CRIT_TIMING IRAM_ATTR
#else
#define CRIT_TIMING 
#endif

void OneWire::begin(uint8_t pin)
{
	pMode(pin, INPUT);
	bitmask = PIN_TO_BITMASK(pin);
	baseReg = PIN_TO_BASEREG(pin);
#if ONEWIRE_SEARCH
	reset_search();
#endif
}


// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
bool CRIT_TIMING OneWire::reset(void)
{
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	__attribute__((unused)) volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	bool r;
	if (!DIRECT_READ(reg, mask)) {
		volatile uint8_t retries = 255;
		while (!DIRECT_READ(reg, mask)) {
			if (retries-- == 0) return false; // wait until the wire is high... just in case
			delayMicroseconds(1);
		}; 
	}
	DIRECT_MODE_OUTPUT(reg, mask);	// drive output low
	delayMicroseconds(480);
	DIRECT_MODE_INPUT(reg, mask);	// allow it to float
	noInterrupts();
	delayMicroseconds(69);
	r = !DIRECT_READ(reg, mask);
	delayMicroseconds(230);
	r &= DIRECT_READ(reg, mask);
	interrupts();
	delayMicroseconds(180);
	return r;
}

//
// Write a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
void CRIT_TIMING OneWire::write_bit(bool v)
{
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	UNUSED volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	noInterrupts();
	DIRECT_MODE_OUTPUT(reg, mask);	// drive output low
	if (v) {
		delayMicroseconds(TIMESLOT_START);
		DIRECT_MODE_INPUT(reg, mask);	//// drive output high
		delayMicroseconds(TIMESLOT - TIMESLOT_START);
	} else {
		delayMicroseconds(TIMESLOT_LOW);
		DIRECT_MODE_INPUT(reg, mask);	//// drive output high
		delayMicroseconds(TIMESLOT - TIMESLOT_LOW);
	}
	interrupts();
}

//
// Read a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
bool CRIT_TIMING OneWire::read_bit(void)
{
	IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
	UNUSED volatile IO_REG_TYPE* reg IO_REG_BASE_ATTR = baseReg;
	bool r;
	noInterrupts();
	DIRECT_MODE_OUTPUT(reg, mask);
	delayMicroseconds(TIMESLOT_START);
	DIRECT_MODE_INPUT(reg, mask);	// let pin float, pull up will raise
	delayMicroseconds(TIMESLOT_READ);
	r = DIRECT_READ(reg, mask);
	interrupts();
	delayMicroseconds(TIMESLOT - TIMESLOT_READ - TIMESLOT_START);
	return r;
}

//
// Write a byte. The writing code uses the active drivers to raise the
// pin high, if you need power after the write (e.g. DS18S20 in
// parasite power mode) then set 'power' to 1, otherwise the pin will
// go tri-state at the end of the write to avoid heating in a short or
// other mishap.
//
void OneWire::write(uint8_t v/*, bool power  = 1 */) {
	for (uint8_t bitMask = 0x01; bitMask; bitMask <<= 1) 
		write_bit(bitMask & v);
	//if (!power) depower();
}

void OneWire::write_bytes(const uint8_t* buf, uint16_t count/*, bool power /* = 1 */) {
	for (uint16_t i = 0; i < count; i++)
		write(buf[i]);
	//if (!power) depower();
}
/*
void OneWire::depower()
{
	noInterrupts();
	DIRECT_MODE_OUTPUT(baseReg, bitmask);
	DIRECT_WRITE_LOW(baseReg, bitmask);
	interrupts();
}
*/
//
// Read a byte
//
uint8_t OneWire::read() {
	uint8_t bitMask,r = 0;
	for (bitMask = 0x01; bitMask; bitMask <<= 1)
		if (read_bit()) r |= bitMask;
	return r;
}

void OneWire::read_bytes(uint8_t* buf, uint16_t count) {
	for (uint16_t i = 0; i < count; i++) 
		buf[i] = read();
}

//
// Do a ROM select
//
void OneWire::select(const uint8_t(&rom)[8])
{
	write(0x55);           // Choose ROM
	write_bytes(rom, 8);
}

//
// Do a ROM skip
//
void OneWire::skip()
{
	write(0xCC);           // Skip ROM
}

#if ONEWIRE_SEARCH
//
// You need to use this function to start a search again from the beginning.
// You do not need to do it for the first search, though you could.
//
void OneWire::reset_search()
{
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = false;
	LastFamilyDiscrepancy = 0;
	memset(ROM_NO, 0, 8);
}

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
//
void OneWire::target_search(uint8_t family_code)
{
	// set the search state to find SearchFamily type devices
	ROM_NO[0] = family_code;
	memset(&ROM_NO[1], 0, 7);
	LastDiscrepancy = 64;
	LastFamilyDiscrepancy = 0;
	LastDeviceFlag = false;
}

//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
bool OneWire::search(uint8_t* newAddr, bool search_mode /* = true */)
{
	uint8_t id_bit_number = 1, last_zero = 0, rom_byte_number = 0, rom_byte_mask = 1, id_bit, cmp_id_bit,  search_direction;
	bool    search_result = false;
	// if the last call was not the last one
	if (!LastDeviceFlag) {
		// 1-Wire reset
		if (!reset()) {
			LastDiscrepancy = 0;
			LastDeviceFlag = false;
			LastFamilyDiscrepancy = 0;
			return false;
		}
		// issue the search command
		if (search_mode) write(0xF0);   // NORMAL SEARCH
		else write(0xEC);   // CONDITIONAL SEARCH
		// loop to do the search
		do { // read a bit and its complement
			id_bit = read_bit();
			cmp_id_bit = read_bit();
			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1)) break;
			else {// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
					search_direction = id_bit;  // bit write value for search
				else {
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy) 
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					else// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);
					// if 0 was picked then record its position in LastZero
					if (search_direction == 0) {
						last_zero = id_bit_number;
						if (last_zero < 9) // check for Last discrepancy in family
							LastFamilyDiscrepancy = last_zero;
					}
				}
				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;
				// serial number search direction write bit
				write_bit(search_direction);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0) {
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		} while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!(id_bit_number < 65)) {
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;
			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = true;
			search_result = true;
		}
	}
	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !ROM_NO[0]) {
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		LastFamilyDiscrepancy = 0;
		search_result = false;
	}
	else for (uint8_t i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
	return search_result;
}

#endif

#if ONEWIRE_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

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

// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (Use tiny 2x16 entry CRC table)
uint8_t OneWire::crc8(const uint8_t* addr, uint8_t len)
{
	uint8_t crc = 0;
	while (len--) {
		crc = *addr++ ^ crc;  // just re-using crc as intermediate
		crc = pgm_read_byte(dscrc2x16_table + (crc & 0x0f)) ^
			pgm_read_byte(dscrc2x16_table + 16 + ((crc >> 4) & 0x0f));
	}
	return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but a little smaller, than the lookup table.
//
uint8_t OneWire::crc8(const uint8_t* addr, uint8_t len)
{
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
bool OneWire::check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc)
{
	crc = ~crc16(input, len, crc);
	return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

uint16_t OneWire::crc16(const uint8_t* input, uint16_t len, uint16_t crc)
{
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
}
#endif

#endif

// undef defines for no particular reason
#ifdef ARDUINO_ARCH_ESP32
#  undef noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#  undef interrupts() portEXIT_CRITICAL(&mux);}
#endif
// for info on this, search "IRAM_ATTR" at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/general-notes.html 
#undef CRIT_TIMING 
