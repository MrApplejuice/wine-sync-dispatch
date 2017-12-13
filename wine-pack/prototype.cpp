#include <stdint.h>

#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <fstream>

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
	typedef std::vector<std::string> StartParametersList;

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
				return exitCodeChar;
			} else {
				throw ProtocolException("invalid message type");
			}
		}
	}
	
	void serializeStartParameters(
			const StartParametersList& startParameterList) {
		std::string buffer;
		
		int32_t count = startParameterList.size();
		buffer += std::string(
			reinterpret_cast<const char*>(&count),
			sizeof(count));
		
		for (const std::string& p : startParameterList) {
			int32_t str_size = p.size();
			buffer += std::string(
				reinterpret_cast<const char*>(&str_size),
				sizeof(str_size));
			buffer += p;
		}
		
		comSocket->send(buffer);
	}
	
	CommunicationProtocol(
			ComSocket* socket,
			const StartParametersList& startParameterList) : 
				stop(false),
				comSocket(socket) {
		serializeStartParameters(startParameterList);
		writeThread = std::thread(std::bind(&CommunicationProtocol::writeLoop, this));
	}
	
	virtual ~CommunicationProtocol() {
		stop = true;
		if (writeThread.joinable()) {
			writeThread.join();
		}
	}
};


class ConfigFileException : public std::exception {
public:
	const std::string error;

	ConfigFileException(const std::string& error) : error(error) {
	}
};

static std::string strip_str(std::string str) {
	const std::string whitespace(" \t\n\r");
	int start = 0;
	while (start < str.size() && whitespace.find(str[start]) != std::string::npos) {
		start++;
	}
	
	int end = str.size() - 1;
	while (end > 0 && whitespace.find(str[end]) != std::string::npos) {
		end--;
	}
	if (end < start) {
		end = start;
	}
	
	return str.substr(start, end - start + 1);
}

class ConfigFile {
public:
	std::string ipAddress;
	int port;
	CommunicationProtocol::StartParametersList args;

	ConfigFile(const std::string& confFilePath) {
		ipAddress ="";
		port = -1;
		
		std::fstream openFile(confFilePath, std::ios_base::in);
		if (!openFile.is_open()) {
			throw ConfigFileException("missing config file: " + confFilePath);
		}
		for (std::string line; std::getline(openFile, line); ) {
			line = strip_str(line);
			if (line.size() == 0 || line.find("#") == 0) {
			} else if (line.find("IP=") == 0) {
				ipAddress = line.substr(3);
			} else if (line.find("PORT=") == 0) {
				port = atoi(line.substr(5).c_str());
			} else if (line.find("ARG=") == 0) {
				args.push_back(line.substr(4));
			} else {
				throw ConfigFileException("invalid argument: " + line);
			}
		}
		
		if (ipAddress == "") {
			throw ConfigFileException("no ip address defined");
		}
		if (!(port > 0 && port < 65536)) {
			throw ConfigFileException("no valid port");
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
	int resultCode = 0;
	try {
		ConfigFile configFile(std::string(argv[0]) + ".conf");
		
		WSAInitializer wsaInit;
		
		SocketClient socket(configFile.ipAddress.c_str(), configFile.port);
		
		CommunicationProtocol::StartParametersList paramList =
			configFile.args;
		for (int i = 1; i < argc; i++) {
			paramList.push_back(argv[i]);
		}
		
		resultCode = CommunicationProtocol(
			&socket,
			paramList).readLoop();
	}
	catch (const ConfigFileException& se) {
		std::cerr << "wine-pack config error: " << se.error << std::endl;
		return PROGRAM_ERROR_CODE;
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
