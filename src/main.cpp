// include libraries 
#include <devices.h>
#include <wifi_loc.h>

// conditional compilation
#define SERIAL_PORT // use serial port;
#define JSON_HANDLER // set of json data notation at wireless transfer;

/// @brief Microcontroller initializing.
void setup() {
	#ifdef SERIAL_PORT
		Serial.begin(115200);
	#endif

	initiate_wifi(&WiFi, &wifi_configuration);

    initiate_ltc2636();
	initiate_si5351();
    initiate_parameters();
}

void loop() {

}