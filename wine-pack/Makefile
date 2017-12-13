.PHONY: all

CXX=i686-w64-mingw32-g++-win32

all: prototype.exe

prototype.exe: prototype.cpp
	$(CXX) -O2 --std=c++14 -o $@ $? -lws2_32 -pthread -Imingw-std-threads/