#include <OneWire.h> // http://www.pjrc.com/teensy/td_libs_OneWire.html
#if defined (__AVR__)
#define LED 13
#define ibuttonpin 2
#define PIN 2
#elif defined CONFIG_IDF_TARGET_ESP32
#define LED 2
#define ibuttonpin 32
#elif defined CONFIG_IDF_TARGET_ESP32C3
#define LED 8
#define ibuttonpin 3
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
const uint8_t Pin_Onewire = ibuttonpin;
OneWire ibutton;
byte key[8];
const byte rom[8] = { 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };
bool led_state = LED_OFF;

void led_blink(byte count = LED_BLINK_COUNT, byte led_delay = LED_DELAY) {
	for (byte i = 0; i < count * 2; i++) {
		digitalWrite(LED, led_state = !led_state);
		delay(led_delay);
	}
}

void setup() {
	pinMode(LED, OUTPUT);
	digitalWrite(LED, led_state);
	Serial.begin(115200);
	led_blink();
	Serial.println("\nReady");
}


void keyvalidcheck() {
	//byte rom[8] = { 0x1, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x0, 0xE1 };
	for (byte i = 0; i < 8; i++)
		if (key[i] != rom[i]) return;
	Serial.print("\tKey is valid!");
	led_blink();
}
void printkey(bool valid_check) {
	Serial.print("ROM \t");
	for (byte i = 0; i < 8; i++) {
#ifdef ARDUINO_ARCH_ESP32
		Serial.printf("%02X ", key[i]);
#else
		Serial.print(' '); Serial.print(key[i], HEX);
#endif
	}
	if (ibutton.crc8(key, 7) != key[7])
		Serial.print("\tCRC is not valid!");
	if (valid_check ) keyvalidcheck();
	Serial.println();
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
	for (int i = 0; i < 8; i++) {
		// формирование 4-х байт для записи в ключ - см. рис.4 из datasheet для подробностей
		ibutton.reset(); // сброс ключа 
		key[0] = 0x3C; // отправляем команду "копировать из буфера в ПЗУ"
		key[1] = i; // указываем байт для записи
		key[2] = 0;
		key[3] = rom[i];
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
void writeByte(byte data) {
	byte data_bit = 0;
	for (; data_bit < 8; data_bit++) {
		if (data & 1) {
			digitalWrite(ibuttonpin, LOW);
			pinMode(ibuttonpin, OUTPUT);
			delayMicroseconds(60);
			pinMode(ibuttonpin, INPUT);
			digitalWrite(ibuttonpin, HIGH);
			delay(10);
		}
		else {
			digitalWrite(ibuttonpin, LOW);
			pinMode(ibuttonpin, OUTPUT);
			delayMicroseconds(5);
			pinMode(ibuttonpin, INPUT);
			digitalWrite(ibuttonpin, HIGH);
			delay(10);
		}
		data = data >> 1;
	}
	return;
}
void write1990() {
	Serial.print("\t");

	ibutton.write(0xD1); // разрешаем запись, отправляем бит 0
	digitalWrite(ibuttonpin, LOW);
	pinMode(ibuttonpin, OUTPUT);
	delayMicroseconds(60);
	pinMode(ibuttonpin, INPUT);
	digitalWrite(ibuttonpin, HIGH);
	delay(10);

	ibutton.reset();
	ibutton.write(0xD5); // команда записи
	for (byte i = 0; i < 8; i++) {
		writeByte(key[i]);
		Serial.print("*");
	}
	Serial.print("\n");
	ibutton.reset();

	ibutton.write(0xD1); // команда выхода из режима записи
	digitalWrite(ibuttonpin, LOW);
	pinMode(ibuttonpin, OUTPUT);
	delayMicroseconds(10);
	pinMode(ibuttonpin, INPUT);
	digitalWrite(ibuttonpin, HIGH);
	delay(10);
	Serial.println("Success!");
	delay(2000);
}
void read_rom() {
	while (!ibutton.reset());
	ibutton.write(0x33);
	ibutton.read_bytes(key, 8);
}
void loop() {
	while (!ibutton.search(key)){};
	//read_rom();
	digitalWrite(LED, led_state = !led_state);
	printkey(1);
	//write1990();
	//ibutton.reset_search();
	digitalWrite(LED, led_state = !led_state);
	//delay(1000);
}
