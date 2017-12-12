#include <stdint.h>

#include <string>
#include <iostream>

#include <winsock2.h>

#define PROGRAM_ERROR_CODE 99

class SocketException : public std::exception {
public:
	const std::string error;

	SocketException(const std::string& error) : error(error) {
	}
};

class SocketClient {
private:
	SOCKET _socket;
	sockaddr_in targetAddress;
public:
	SocketClient(const char* ip_address, int port) : 
			_socket(INVALID_SOCKET) {
		unsigned long ip_addr = inet_addr(ip_address);
		if (ip_addr == INADDR_NONE) {
			throw SocketException("invalid ip address specified");
		}
		
		memset(&targetAddress, 0, sizeof(targetAddress));
		targetAddress.sin_family = AF_INET;
		targetAddress.sin_addr.s_addr = ip_addr;
		targetAddress.sin_port = htons(port);
			
		_socket = socket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);
		if (_socket == INVALID_SOCKET) {
			throw SocketException("socket cannot be initialized");
		}
		
		if (connect(
				_socket,
				reinterpret_cast<sockaddr*>(&targetAddress),
				sizeof(targetAddress)) != 0) {
			throw SocketException("socket cannot connect");
		}
	}
	
	virtual void send(const std::string& data) {
		if (::send(_socket, data.c_str(), data.size(), 0) != data.size()) {
			throw SocketException("cannot send data");
		}
	}
	
	virtual std::string read(int len) {
		char* buffer = new char[len];
		const int r = ::recv(_socket, buffer, len, 0);
		if (r == SOCKET_ERROR) {
			delete[] buffer;
			throw SocketException("Error while receving data");
		}
		std::string result(buffer, r);
		delete[] buffer;
		
		return result;
	}
	
	virtual ~SocketClient() {
		if (_socket != INVALID_SOCKET) {
			closesocket(_socket);
		}
	}
};


class WSAInitializer {
public:
	bool initialized;
	WSADATA wsaData;

	WSAInitializer() : initialized(false) {
		uint16_t wVersionRequested = MAKEWORD( 2, 2 );
		if (WSAStartup(wVersionRequested, &wsaData) != 0) {
			throw SocketException("Could not initialize windows sockets"); 
		}
		initialized = true;
	}
	
	virtual ~WSAInitializer() {
		if (initialized) {
			WSACleanup();
		}
	}
};

int main(int argc, const char** argv) {
	try {
		WSAInitializer wsaInit;
		
		SocketClient socket("127.0.0.1", 8888);
		socket.send("shitters it works\n");
		std::cout << "receiving: " << std::flush << socket.read(1000) << std::endl;
	}
	catch (const SocketException& se) {
		std::cerr << "wine-pack: " << se.error << std::endl;
		return PROGRAM_ERROR_CODE;
	}

	return 0;
}
