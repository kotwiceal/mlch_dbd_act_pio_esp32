// arduino framework libraries
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

// supporting libraries
#include <vector>
#include <map>
#include <functional>

// define commands
#define SETVOL 0b0011 // SPI command of setting voltage;

// initialization setting of SPI transfer
SPISettings settings(10000000, MSBFIRST, SPI_MODE2);

// initialization of value on LTC3636
std::vector<float> voltage(16, 0); // [mV];

// initialization of map index
uint8_t voltage_channel_to_device_index[16] = {4, 4, 4, 4, 4, 4, 4, 4, 16, 16, 16, 16, 16, 16, 16, 16};
uint8_t voltage_channel_to_channel_index[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};

// define pinouts of esp32 board
int8_t // pinout for LTC3636;
	pin_spi_cs_0 = 4,
	pin_spi_cs_1 = 16,
	pin_spi_sck = 19,
	pin_spi_mosi = 18,
	pin_spi_miso = 23;

/// @brief Initialize pins for SPI interface.
void initiate_ltc2636 () {
    pinMode(pin_spi_cs_0, OUTPUT);
    pinMode(pin_spi_cs_1, OUTPUT);
    pinMode(pin_spi_sck, OUTPUT);
	pinMode(pin_spi_mosi, OUTPUT);
	pinMode(pin_spi_miso, INPUT);

	digitalWrite(pin_spi_cs_0, LOW);
	digitalWrite(pin_spi_cs_1, LOW);
	digitalWrite(pin_spi_sck, LOW);
	digitalWrite(pin_spi_mosi, LOW);
}

/**
 * @brief Set given value on given channel of LTC3636 cascade by means SPI interface.
 * @param channel is index of cascaded channel [0-31];
 * @param value is assigned value [0-4000[mV]];
*/
void set_voltage (uint8_t channel, float value) {
	uint8_t pin_spi_cs = voltage_channel_to_device_index[channel];
	uint8_t channel_index = voltage_channel_to_channel_index[channel];

	SPI.begin(pin_spi_sck, pin_spi_miso, pin_spi_mosi, pin_spi_cs);

	SPI.beginTransaction(settings);
	digitalWrite(pin_spi_cs, HIGH);
	SPI.transfer(~((SETVOL << 4) | channel_index));
	SPI.transfer16(~(int(value) << 4));
	digitalWrite(pin_spi_cs, LOW);
	SPI.endTransaction();

	#ifdef SERIAL_PORT
		Serial.println("dac[" + String(channel) + "]="+String(value));
	#endif
}

/// @brief Initialize devices by setting initial value to each channel.
void initiate_parameters () {
	for (int i = 0; i < voltage.size(); i++) {set_voltage(i, voltage[i]);}
}