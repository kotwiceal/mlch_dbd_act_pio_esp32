// arduino framework libraries
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

// supporting libraries
#include <vector>
#include <map>
#include <functional>

#define HTTP_OK 200

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
