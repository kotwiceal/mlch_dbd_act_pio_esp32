// include libraries 
#include <devices.h>

// conditional compilation
#define SERIAL_PORT // use serial port;

/// @brief Microcontroller initializing.
void setup() {
	#ifdef SERIAL_PORT
		Serial.begin(115200);
	#endif

    initiate_ltc2636();
	initiate_si5351();
    initiate_parameters();
}

void loop() {

}