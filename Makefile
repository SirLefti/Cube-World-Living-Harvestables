CXX = g++

CXXFLAGS = -O2 -masm=intel -Wall
LDFLAGS = -shared -static -static-libgcc -static-libstdc++ -s

HEADERS = target_client.h target_server.h

.PHONY: all
all: LivingHarvestables.dll LivingHarvestablesServer.dll

LivingHarvestables.dll: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

LivingHarvestablesServer.dll: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -DTARGET_SERVER -o $@ $< $(LDFLAGS)
