.PHONY: all

CXX=i686-w64-mingw32-g++-win32

all: wine-pack.exe

wine-pack.exe: wine-pack.cpp
	$(CXX) -O2 --std=c++14 -o $@ $? -lws2_32 -pthread -Imingw-std-threads/