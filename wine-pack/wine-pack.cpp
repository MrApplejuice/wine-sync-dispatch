#include <stdint.h>

#include <string>
#include <iostream>
#include <functional>

#include <winsock2.h>

#ifdef _GLIBCXX_HAS_GTHREADS
#	include <thread>
#else
#	include <mingw.thread.h>
#endif

#include <windows.h>

#define PROGRAM_ERROR_CODE 99


class SocketException : public std::exception {
public:
	const std::string error;

	SocketException(const std::string& error) : error(error) {
	}
};

class ComSocket {
public:
	virtual void send(const std::string& data) = 0;
	virtual std::string read(int len) = 0;
};

class SocketClient : public ComSocket {
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
	
	virtual void send(const std::string& data) override {
		if (::send(_socket, data.c_str(), data.size(), 0) != data.size()) {
			throw SocketException("cannot send data");
		}
	}
	
	virtual std::string read(int len) override {
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


class ProtocolException : public std::exception {
public:
	const std::string error;

	ProtocolException(const std::string& error) : error(error) {
	}
};

class CommunicationProtocol {
private:
	bool stop;
public:
	ComSocket* comSocket;
	std::thread writeThread;
	
	virtual void writeLoop() {
		HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
		char* buffer = new char[1024];
		
		while (!stop) {
			// thanks to https://stackoverflow.com/a/43808444/1775639
			if (WaitForSingleObject(h, 100) == WAIT_OBJECT_0) {
				std::cin.readsome(buffer, 1024);
				int readLength = std::cin.gcount();
				if (readLength <= 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				} else {
					std::cout << "Read " << readLength << std::endl;
					comSocket->send(
						"s" + 
						std::string(reinterpret_cast<const char*>(&readLength), 4) +
						std::string(buffer, readLength));
				}
			}
		}
		
		delete[] buffer;
	}
	
	virtual int readLoop() {
		while (true) {
			const std::string type = comSocket->read(1);
			
			if (type == "e") {
				const std::string exitCodeByte = comSocket->read(1);
				if (exitCodeByte == "") {
					throw ProtocolException("missing exitcode");
				}
				const char exitCodeChar = exitCodeByte[0];
				std::cout << "attempting to exit program: " << (int) exitCodeChar << std::endl;
				return exitCodeChar;
			} else {
				throw ProtocolException("invalid message type");
			}
		}
	}
	
	CommunicationProtocol(ComSocket* socket) : 
			stop(false),
			comSocket(socket),
			writeThread(std::bind(&CommunicationProtocol::writeLoop, this)) {
	}
	
	virtual ~CommunicationProtocol() {
		stop = true;
		writeThread.join();
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
	int resultCode = 0;
	try {
		WSAInitializer wsaInit;
		
		SocketClient socket("127.0.0.1", 8888);
		resultCode = CommunicationProtocol(&socket).readLoop();
	}
	catch (const ProtocolException& se) {
		std::cerr << "wine-pack protocol error: " << se.error << std::endl;
		return PROGRAM_ERROR_CODE;
	}
	catch (const SocketException& se) {
		std::cerr << "wine-pack socket error: " << se.error << std::endl;
		return PROGRAM_ERROR_CODE;
	}

	return resultCode;
}
