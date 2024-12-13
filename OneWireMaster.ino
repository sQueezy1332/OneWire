#include <OneWire.h> // http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <stdio.h>
#if defined (__AVR__)
#define LED 13
#define ibuttonpin 4
#define pullup_pin 9
#define PIN 2
#elif defined CONFIG_IDF_TARGET_ESP32
#define LED 2
#define ibuttonpin 32
#define pullup_pin 9
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define LED 8
#define ibuttonpin 3
#define pullup_pin 9
#endif
#if defined CONFIG_IDF_TARGET_ESP32C3
#define LED_ON	0
#define LED_OFF	1
#else
#define LED_ON	1
#define LED_OFF	0
#endif
#define LED_DELAY 50
#define LED_BLINK_COUNT 5

typedef const byte cbyte;
const uint8_t pin_onewire = ibuttonpin;
OneWire ibutton;

static const byte rom[8] = { 0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0xE1 };
static const byte rom_em[8] = { 0x01, 0x62, 0xBA, 0x5D, 0x00, 0x0A, 0x00, 0xC0 };
static byte key[8];
uint64_t cards[23];

void led_blink(byte count = LED_BLINK_COUNT, byte led_delay = LED_DELAY) {
	while (count--) {
		digitalWrite(LED, LED_ON);
		delay(led_delay);
		digitalWrite(LED, LED_OFF);
		delay(led_delay);
	}
}

void setup() {
	pinMode(LED, OUTPUT);
	digitalWrite(LED, LED_OFF);
	pinMode(pullup_pin, OUTPUT);
	digitalWrite(pullup_pin, 1);
	Serial.begin(250000);
	led_blink();
	Serial.println("\nReady");
}

bool keyCompare(cbyte(&buf)[8], cbyte(&com)[8]) {
	for (byte i = 0; i < 8; i++) if (buf[i] != com[i]) return false;
	return true;
}

void sortingArray(uint64_t buf[], byte& keyReaded) {
	byte next = 0;
	const byte size = sizeof(cards[0]);
	for (byte first = 0, second; first < keyReaded; first++) {
		if (buf[first] == 0) continue;
		for (second = first + 1; second < keyReaded; second++) {
			if (buf[second] == 0) continue;
			if (buf[first] == buf[second])  buf[second] = 0;
			else buf[first + 1] = buf[second];
		}
		if (buf[first] == 0) {
			for (; next < keyReaded; next++) {
				if (buf[next]) {
					buf[first] = buf[next];
					buf[next] = 0; break;
				}
			}
			if (next == keyReaded) {
				keyReaded = first;
				return;
			}
		}
		for (second = ++next; second < keyReaded; second++) {
			if (buf[second]) continue;
			if (!(buf[first] != buf[second]))
				buf[second] = 0;
		}
	}
}

void printkey(cbyte(&buf)[8] = key) {
	//Serial.print("\nROM\t");
	for (byte i = 0; i < 8; i++) {
#ifdef ARDUINO_ARCH_ESP32
		Serial.printf("%02X ", buf[i]);
#else	
		Serial.print(' ');
		if ((buf[i] & 0xF0) == 0) Serial.print('0');
		Serial.print(buf[i], HEX);
#endif
}
	byte crc = ibutton.crc8(buf, 7); //
	if (crc != buf[7]) {
		Serial.print("\tCRC not valid!\t");
		Serial.print(crc, HEX);
	}
}

void send_programming_impulse() {
	digitalWrite(ibuttonpin, HIGH);
	delay(60);
	digitalWrite(ibuttonpin, LOW);
	delay(5);
	digitalWrite(ibuttonpin, HIGH);
	delay(50);
}

void writekey() {
	Serial.print("\tStart programming..."); // начало процесса записи данных в ключ
	//printkey();
	byte b;
	for (byte i = 0; i < 8; i++) {
		// формирование 4-х байт для записи в ключ - см. рис.4 из datasheet для подробностей
		key[0] = 0x3C; // отправляем команду "копировать из буфера в ПЗУ"
		key[1] = i; // указываем байт для записи
		key[2] = 0;
		key[3] = rom[i];
		while (!ibutton.reset()); // сброс ключа 
		ibutton.write_bytes(key, 4); // записываем i-ый байт в ключ

		b = ibutton.read(); // считываем байт из ключа

		if (OneWire::crc8(key, 4) != b) { // при ошибке контрольной суммы
			Serial.println("Error while programming!"); // сообщаем об этом
			return; // и отменяем запись ключа
		}
		send_programming_impulse(); // если всё хорошо, посылаем импульс для записи i-го байта в ключ
	}
	Serial.println("Success!"); // сообщение об успешной записи данных в ключ  
}

void writeBit(bool bit) {
	if (bit) {
		pinMode(ibuttonpin, OUTPUT);
		delayUs(60);
		pinMode(ibuttonpin, INPUT);
		delay(10);
	}
	else {
		pinMode(ibuttonpin, OUTPUT);
		delayUs(5);
		pinMode(ibuttonpin, INPUT);
		delay(10);
	}
}

void writeByte(byte data) {
	for (byte bitmask = 1; bitmask; bitmask <<= 1) {
		writeBit(data & bitmask);
	}
}

void read_rom(byte buf[] = key, byte count = 8) {
	while (!ibutton.reset()) {
		//while(!digitalRead(ibuttonpin)) delay(100); 
	};
	// delay(10000);
	ibutton.write(0x33);
	ibutton.read_bytes(buf, count);
	Serial.print("\n0x33\t");
}

void write1990(cbyte(&buf)[8] = rom) {
	Serial.println("\nWait key to write ...");
	while (!ibutton.reset());
	Serial.print('\t');
	ibutton.write(0xD1);	//разрешаем запись
	writeBit(0);			//отправляем бит 0
	while (!ibutton.reset());
	ibutton.write(0xD5);	//команда записи
	for (byte i = 0; i < 8; i++) {
		writeByte(buf[i]);
		Serial.print('*');
	}
	Serial.println();
	while (!ibutton.reset());
	ibutton.write(0xD1);	//команда выхода из режима записи
	writeBit(1);
	read_rom();
	Serial.println(keyCompare(key, buf) ? "Success!" : "Error!");
	delay(2000); //Serial
}

void print_zero(byte& count) { for (; count; count--) Serial.print('0'); }
void print_em() {
	uint32_t dez10 = *(uint32_t*)(key + 1);
	byte facility = (dez10 >> 16);
	byte zero = 0;
	if (dez10 < 10000000ul) zero = 3;
	else if (dez10 < 100000000ul) zero = 2;
	else if (dez10 < 1000000000ul) zero = 1;
	Serial.print('\t');
	print_zero(zero);
	Serial.print(dez10); Serial.print('\t');
	if (facility == 0) zero = 3;
	else if (facility < 10) zero = 2;
	else if (facility < 100) zero = 1;
	else zero = 0;
	print_zero(zero);
	Serial.print(facility);
	Serial.print(',');
	Serial.print((uint16_t)dez10);
	//Serial.println();
}
uint32_t timestamp;
void loop() {
	
	//timestamp = micros();
	//for (byte count = 0;; count++) {
	//	read_rom();
	//	if (count == 23) { timestamp = micros() - timestamp; Serial.println(); Serial.println(timestamp); return; }
	//	
	//}
	//while (!ibutton.search(key)){};
	//while (digitalRead(ibuttonpin)) {};
	for (byte i = 0;i < 23;i++) {
		
		read_rom();
		digitalWrite(LED, LED_ON);
		printkey();
		digitalWrite(LED, LED_OFF);
	}
	delay(1000);
	//digitalWrite(LED, LED_ON);
	//if (keyCompare(key, rom_em)) { pinMode(ibuttonpin, OUTPUT); timestamp = millis(); Serial.print("\tKey is valid!"); }
	//printkey();// delay(500);
	//print_em();
	//if (keyCompare(key, rom)) { Serial.print("\tKey is valid!");  led_blink(); }
	//if (!digitalRead(ibuttonpin)) { while (millis() - timestamp < 2500) { led_blink(); }; pinMode(ibuttonpin, INPUT); }
	//write1990();

	//ibutton.reset_search();
	//digitalWrite(LED, LED_OFF);
	//delay(1000);
}
