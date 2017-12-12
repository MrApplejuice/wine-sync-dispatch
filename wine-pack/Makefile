.PHONY: all

CXX=i686-w64-mingw32-g++-win32

all: wine-pack.exe

wine-pack.exe: wine-pack.cpp
	$(CXX) --std=c++11 -o $@ $? -lws2_32