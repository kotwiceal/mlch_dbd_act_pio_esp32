// include libraries 
#include <devices.h>
#include <wifi_loc.h>
#include <udp_server_loc.h>
#include <http_server_loc.h>

// conditional compilation
#define SERIAL_PORT // use serial port;
#define JSON_HANDLER // set of json data notation at wireless transfer;
#define UDP_SOCKET // create udp socket and handle receiving packets;
#define HTTP_SERVER // create http server and handle requests;

#ifdef UDP_SOCKET
	#define UDP_PORT 8080
#endif

#ifdef HTTP_SERVER
	#define HTTP_PORT 8090
#endif

/// @brief Microcontroller initializing.
void setup() {
	#ifdef SERIAL_PORT
		Serial.begin(115200);
	#endif

	initiate_wifi(&WiFi, &wifi_configuration);

	#ifdef UDP_SOCKET
		#ifdef JSON_HANDLER
			AsyncUDP* upd_sock = initiate_udp_socket(UDP_PORT, json_handler);
		#endif
	#endif

	#ifdef HTTP_SERVER
		#ifdef JSON_HANDLER
			AsyncWebServer* http_server = initiate_http_server(HTTP_PORT, &parameters, json_handler);
		#endif
	#endif

    initiate_ltc2636();
	initiate_si5351();
    initiate_parameters();
}

void loop() {

}