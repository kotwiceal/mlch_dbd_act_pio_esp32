// arduino framework libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <si5351.h>

// supporting libraries
#include <vector>
#include <map>
#include <functional>

// define commands
#define SETVOL 0b0011 // SPI command of setting voltage;
#define TCAADDR_0 0x70 // I2C address of TCA9548A;
#define TCAADDR_1 0x71 // I2C address of TCA9548A at supplying an address pin;

// initialization setting of SPI transfer
SPISettings settings(10000000, MSBFIRST, SPI_MODE2);

// initialization of value on LTC3636
std::vector<float> voltage(16, 0), // [mV];
	frequency(16, 60); //[kHz];

// initialization of map index
uint8_t voltage_channel_to_device_index[16] = {4, 4, 4, 4, 4, 4, 4, 4, 16, 16, 16, 16, 16, 16, 16, 16};
uint8_t voltage_channel_to_channel_index[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
uint8_t frequency_channel_to_device_index[16] = {4, 4, 5, 5, 6, 6, 7, 7, 0, 0, 1, 1, 2, 2, 3, 3};
uint8_t frequency_channel_to_channel_index[16] = {0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2};

// define pinouts of esp32 board
int8_t // pinout for LTC3636;
	pin_spi_cs_0 = 4,
	pin_spi_cs_1 = 16,
	pin_spi_sck = 19,
	pin_spi_mosi = 18,
	pin_spi_miso = 23;

int8_t // pinout for TCA9548A -> SI5351;
	pin_rst_0 = 0,
	pin_rst_1 = 2,
	pin_i2c_sda = 21,
	pin_i2c_cls = 22;

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


/// @brief Initialize pins for I2C interface.
void initiate_si5351 () {
	pinMode(pin_rst_0, OUTPUT);
	pinMode(pin_rst_1, OUTPUT);

	digitalWrite(pin_rst_0, HIGH);
	digitalWrite(pin_rst_1, HIGH);

	Wire.begin();

	for (int i = 0; i < 8; i++) {
		Wire.beginTransmission(TCAADDR_1);
		Wire.write(1 << i);
		Wire.endTransmission();
		si5351_Init(978);
		si5351_EnableOutputs((1<<0) | (1<<2));
	}
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

/**
 * @brief Set given frequency on given channel of SI3153 multiplexed cascade by TCA9548A by means I2C interface.
 * @param channel is index of cascaded channel [0-31];
 * @param value is assigned value [8-160[kHz]];
*/
void set_frequency (uint8_t channel, float value) {
	uint8_t device_index = frequency_channel_to_device_index[channel];
	uint8_t channel_index = frequency_channel_to_channel_index[channel];

	Wire.beginTransmission(TCAADDR_1);
	Wire.write(1 << device_index);
	Wire.endTransmission();

	if (channel_index == 0) {
		si5351_SetupCLK0(int(value*1e3), SI5351_DRIVE_STRENGTH_4MA);
	}
	else {
		si5351_SetupCLK2(int(value*1e3), SI5351_DRIVE_STRENGTH_4MA);
	}

	#ifdef SERIAL_PORT
		Serial.println("set_frequency");
	#endif
}


// present variables and functions as associative arrays
std::map<String, std::function<void(uint8_t, float)>> functions = {{"dac", set_voltage}, {"fm", set_frequency}};
std::map<String, std::vector<float>> parameters = {{"dac", voltage}, {"fm", frequency}};

/**
	* @brief Handle JSON packet presenting dictionary with keys 'dac' and 'fm' which values contain dictionary of 
	* two one-dimensional vectors same size with names 'index' as integer and 'value' as float.
	* According to keys 'dac' and 'fm' supporting function is consequently called to assign 
	* value (mV or kHz) by index to device channel via digital interface.

	* @param data serialized JSON data like '{"dac": {"index": [0, 1, 2], "value": [3, 4, 5]}, 
		"fm": {"index": [0, 1, 2], "value": [3, 4, 5]}}'
*/
void json_handler (String data) {
	DynamicJsonDocument document(1024);
	#ifdef SERIAL_PORT
		Serial.println(data);
	#endif
	deserializeJson(document, data);
	JsonObject json_object = document.as<JsonObject>();
	for(JsonPair json_pair: json_object) {
		JsonObject json_pair_value = json_pair.value().as<JsonObject>();
		JsonArray index_array = json_pair_value["index"];
		JsonArray value_array = json_pair_value["value"];
		if (index_array.size() == value_array.size()) {
			for (int i = 0; i < index_array.size(); i++) {
				int8_t index = index_array[i].as<int>();
				float value = value_array[i].as<float>();
				functions[json_pair.key().c_str()](index, value);
				parameters[json_pair.key().c_str()][index] = value;
			}
		}
	}
}

/// @brief Initialize devices by setting initial value to each channel.
void initiate_parameters () {
	for (auto device: parameters) {
		String command = device.first;
		std::vector<float> vector = device.second;
		for (int i = 0; i < vector.size(); i++) {
			functions[command](i, vector[i]);
		}
	}
}