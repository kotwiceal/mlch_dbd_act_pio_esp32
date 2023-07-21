// arduino framework libraries
#include <Arduino.h>
#include <AsyncUDP.h>

// supporting libraries
#include <vector>
#include <map>
#include <functional>

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
        handle(String(packet.data(), packet.length()));
	});
    return udp_server;
}