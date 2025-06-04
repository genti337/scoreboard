TARGET = espn_jsonc
SRC = src/main.cpp src/FetchData.cpp src/ESPNParser.cpp

# Default compiler and flags
CXX = g++
CXXFLAGS = -Wall -O2

# Platform detection
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
    # macOS - assume Homebrew is installed
    JSONC_PREFIX := $(shell brew --prefix json-c)
    CURL_PREFIX := $(shell brew --prefix curl)
    INCLUDES = -I$(JSONC_PREFIX)/include -I$(CURL_PREFIX)/include
    LIBPATHS = -L$(JSONC_PREFIX)/lib -L$(CURL_PREFIX)/lib
else
    # Linux (Raspberry Pi OS)
    INCLUDES =
    LIBPATHS =
endif

LIBS = -ljson-c -lcurl

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBPATHS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)
