TARGET = espn_jsonc
SRC = src/main.cpp src/FetchData.cpp src/ESPNParser.cpp src/Team.cpp src/Competition.cpp src/Display.cpp

# Default compiler and flags
CXX = g++
CXXFLAGS = -Wall -O2 -I../rpi-rgb-led-matrix -I../rpi-rgb-led-matrix/include -I/usr/include/ImageMagick-6 -I/usr/include/arm-linux-gnueabihf/ImageMagick-6

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
    LIBPATHS = -L../rpi-rgb-led-matrix/lib
endif

LIBS = -ljson-c -lcurl -lrgbmatrix -lrt -lm -lpthread -lMagick++-6.Q16 -lMagickWand-6.Q16 -lMagickCore-6.Q16

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(LIBPATHS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)
