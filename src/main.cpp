// include libraries 
// arduino framework libraries
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <si5351.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// supporting libraries
#include <vector>
#include <map>
#include <functional>

// conditional compilation
#define SERIAL_PORT // use serial port;
#define JSON_HANDLER // set of json data notation at wireless transfer;
#define UDP_SOCKET // create udp socket and handle receiving packets;
#define HTTP_SERVER // create http server and handle requests;

#ifdef UDP_SOCKET
	#define UDP_PORT 8080
#endif

#define HTTP_OK 200

#ifdef HTTP_SERVER
	#define HTTP_PORT 8090
	// #define HTTP_STATIC
#endif

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
	#ifdef SERIAL_PORT
		Serial.println("dac[" + String(channel) + "]=" + String(value));
	#endif

	uint8_t pin_spi_cs = voltage_channel_to_device_index[channel];
	uint8_t channel_index = voltage_channel_to_channel_index[channel];

	SPI.begin(pin_spi_sck, pin_spi_miso, pin_spi_mosi, pin_spi_cs);

	SPI.beginTransaction(settings);
	digitalWrite(pin_spi_cs, HIGH);
	SPI.transfer(~((SETVOL << 4) | channel_index));
	SPI.transfer16(~(int(value) << 4));
	digitalWrite(pin_spi_cs, LOW);
	SPI.endTransaction();
}

/**
 * @brief Set given frequency on given channel of SI3153 multiplexed cascade by TCA9548A by means I2C interface.
 * @param channel is index of cascaded channel [0-31];
 * @param value is assigned value [8-160[kHz]];
*/
void set_frequency (uint8_t channel, float value) {
	#ifdef SERIAL_PORT
		Serial.println("fm[" + String(channel) + "]=" + String(value));
	#endif

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
	for (auto& device: parameters) {
		String command = device.first;
		std::vector<float> vector = device.second;
		for (int i = 0; i < vector.size(); i++) {
			functions[command](i, vector[i]);
		}
	}
}


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
 * @param configuration struct with fileds: ssid, password, channel, ssid hidden, max connection.
 * @return wifi instance
*/
WiFiClass* initiate_wifi(wifi_net* configuration) {
	WiFiClass* wifi = new WiFiClass;
	wifi->mode(WIFI_AP);
	wifi->softAP(configuration->ssid, configuration->password, configuration->channel, 
		configuration->ssid_hidden, configuration->max_connection);
	wifi->softAPConfig(configuration->local_ip, configuration->gateway, configuration->subnet);
	#ifdef SERIAL_PORT
		Serial.printf("MAC address = %s\n", wifi->softAPmacAddress().c_str());
		Serial.println("IP address = " + wifi->localIP().toString());
	#endif
	wifi->onEvent([wifi] (WiFiEvent_t event, WiFiEventInfo_t info) {
		if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
			#ifdef SERIAL_PORT
				Serial.printf("Stations connected, number of soft-AP clients = %d\n", wifi->softAPgetStationNum());
			#endif
		}
		if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
			#ifdef SERIAL_PORT
				Serial.printf("Stations disconnected, number of soft-AP clients = %d\n", wifi->softAPgetStationNum());
			#endif
		}
	});
	return wifi;
}

/**
 * @brief Initiate UDP socket.
 * @param port 
 * @param handle function called at receiving by server a packet. 
 * @return udp server instance
*/
AsyncUDP* initiate_udp_socket (uint16_t port, std::function<void(String)> handle) {
	// create socket instance
	AsyncUDP *udp_server = new AsyncUDP;
	udp_server->listen(port);
	
	// handle receiving of packet
	udp_server->onPacket([handle] (AsyncUDPPacket packet) {
		Serial.println(String(packet.data(), packet.length()));
        handle(String(packet.data(), packet.length()));
	});
    return udp_server;
}

struct WebFile {
    String url, path, type;
};

/**
 * @brief Scaning files in given directory.
 * @param fs filesystem instance
 * @param dirname catalog name of scaning
 * @return pathes vector
*/
std::vector<String> get_pathes(fs::FS* fs, const char * dirname){
	std::vector<String> pathes;
    File root = fs->open(dirname);
    if(!root){
        return pathes;
    }
    if(!root.isDirectory()){
        return pathes;
    }
    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory()){
			pathes.push_back(file.name());
        }
        file = root.openNextFile();
    }
	return pathes;
}

/**
 * @brief Scaning files in given directory.
 * @param spifs filesystem instance
 * @return vector of routed files
*/
std::vector<WebFile> initiate_file_system (fs::SPIFFSFS* spifs) {
	// start SPI filesystem interface
    spifs->begin();

	// scaning of root directory
    std::vector<String> pathes = get_pathes(spifs, "/");

	std::vector<WebFile> webfiles; WebFile temporary; String extension; 
    for (String path: pathes) {
        temporary.path = path;
        temporary.url = path;
		extension = path.substring(path.lastIndexOf(".") + 1, path.length());
        if (extension == "html") {
            if (path == "/index.html") {
                temporary.url = "/";
            } 
            temporary.type = "text/html";
        }
        if (extension == "css") {
            temporary.type = "text/css";
        }
        if (extension == "js") {
            temporary.type = "text/javascript";
        }
        webfiles.push_back(temporary);
    }
	return webfiles;
}

/**
 * @brief Initiate HTTP server.
 * @param port
 * @param parameters presents associative labeled array that each value is 1D float array
 * @param handle function called at receiving by server a packet
*/
AsyncWebServer* initiate_http_server (uint16_t port, std::map<String, std::vector<float>> *parameters, std::function<void(String)> handle) {
    // create a server instance
	AsyncWebServer *http_server = new AsyncWebServer(port);

    // handle event connection
    AsyncEventSource *handler_events = new AsyncEventSource("/events");
	handler_events->onConnect([](AsyncEventSourceClient *client) {
		#ifdef SERIAL_PORT
			Serial.println("Client status: " + String(client->connected()));
		#endif
    });

    // handle request
	AsyncCallbackJsonWebHandler* handler_set_param = new AsyncCallbackJsonWebHandler("/set-param", [handle](AsyncWebServerRequest *request, JsonVariant &json) {
		#ifdef SERIAL_PORT
			Serial.println("request http://hostname:port/set-param" + json.as<String>());
		#endif
        handle(json.as<String>());
		request->send(HTTP_OK, "text/plain");
    });

    // deserialize parameters variable to JSON notation
	http_server->on("/get-param", HTTP_GET, [parameters](AsyncWebServerRequest *request) {
		DynamicJsonDocument json(1024);
		JsonObject json_object = json.createNestedObject();
		
		for (auto& element: *parameters) {
			json_object[element.first]["value"] = json.createNestedArray();
			for (auto& value: element.second) {
				json_object[element.first]["value"].add(value);
			}
		}

		String response;
		serializeJson(json_object, response);

		#ifdef SERIAL_PORT
			Serial.println(response);
		#endif

		request->send(HTTP_OK, "application/json", response);
	});

    // append handlers to server
	http_server->addHandler(handler_events);
    http_server->addHandler(handler_set_param);
	http_server->begin();

    return http_server;
}

/**
 * @brief Route static files stored in flash memory.
 * @param http_server async HTTP server instance
 * @param spiffs SPIFFS instance
 * @param webfiles configuration of routed files
*/
void route_static_files(AsyncWebServer* http_server, fs::SPIFFSFS* spiffs, std::vector<WebFile>* webfiles) {
    for (WebFile webfile: *webfiles) {
        char url[webfile.url.length() + 1];
        webfile.url.toCharArray(url, webfile.url.length() + 1);
        http_server->on(url, HTTP_GET, [webfile, spiffs](AsyncWebServerRequest *request) {
            request->send(*spiffs, webfile.path, webfile.type);
        }); 
    }
}

/// @brief Microcontroller initializing.
void setup() {
	#ifdef SERIAL_PORT
		Serial.begin(115200);
		Serial.println("\n");
	#endif

	WiFiClass* wifi = initiate_wifi(&wifi_configuration);

	#ifdef UDP_SOCKET
		#ifdef JSON_HANDLER
			AsyncUDP* upd_sock = initiate_udp_socket(UDP_PORT, json_handler);
		#endif
	#endif

	#ifdef HTTP_SERVER
		#ifdef JSON_HANDLER
			AsyncWebServer* http_server = initiate_http_server(HTTP_PORT, &parameters, json_handler);
			#ifdef HTTP_STATIC
				std::vector<WebFile> webfiles = initiate_file_system(&SPIFFS);
				route_static_files(http_server, &SPIFFS, &webfiles);
			#endif
		#endif
	#endif

    initiate_ltc2636();
	initiate_si5351();
    initiate_parameters();
}

void loop() {

}