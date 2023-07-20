// arduino framework libraries
#include <Arduino.h>
#include <WiFi.h>

// define configuration of WiFi network;
struct wifi_net {
	const char* ssid = "esp32_dbd";
	const char* password = "qwerty123";
	const int channel = 1;
	const int ssid_hidden = 0;
	const int max_connection = 1;
	IPAddress local_ip = IPAddress(192, 168, 1, 1);
	IPAddress gateway = IPAddress(192, 168, 1, 1);
	IPAddress subnet = IPAddress(255, 255, 255, 0);
} wifi_configuration;

/**
 * @brief Initialize WiFi network according to configurations.
 * @param WiFi is class instance;
 * @param configuration struct with fileds: ssid, password, channel, ssid hidden, max connection.
*/
void initiate_wifi (WiFiClass* WiFi, wifi_net* configuration) {
	WiFi->mode(WIFI_AP);
	WiFi->softAP(configuration->ssid, configuration->password, configuration->channel, 
		configuration->ssid_hidden, configuration->max_connection);
	WiFi->softAPConfig(configuration->local_ip, configuration->gateway, configuration->subnet);
	#ifdef SERIAL_PORT
		Serial.printf("MAC address = %s\n", WiFi->softAPmacAddress().c_str());
		Serial.println("IP address = " + WiFi->localIP());
	#endif
	WiFi->onEvent([&WiFi] (WiFiEvent_t event, WiFiEventInfo_t info) {
		if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
			#ifdef SERIAL_PORT
				Serial.printf("Stations connected, number of soft-AP clients = %d\n", WiFi->softAPgetStationNum());
			#endif
		}
		if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
			#ifdef SERIAL_PORT
				Serial.printf("Stations disconnected, number of soft-AP clients = %d\n", WiFi->softAPgetStationNum());
			#endif
		}
	});
}