TARGET = espn_jsonc

SRCDIR = src
OBJDIR = build

SRC = $(wildcard $(SRCDIR)/*.cpp)
OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRC))

UNAME := $(shell uname)

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17
LDFLAGS =
LIBS = -lcurl -ltinyxml2 -lm -lpthread

CXXFLAGS += $(shell pkg-config --cflags Magick++)
LIBS += $(shell pkg-config --libs Magick++)

ifeq ($(UNAME), Darwin)
    CURL_PREFIX := $(shell brew --prefix curl)
    TINYXML2_PREFIX := $(shell brew --prefix tinyxml2)
    SDL2_PREFIX := $(shell brew --prefix sdl2)

    CXXFLAGS += -DMAC_STUB_MATRIX
    CXXFLAGS += -Imac_stub
    CXXFLAGS += -I$(CURL_PREFIX)/include
    CXXFLAGS += -I$(TINYXML2_PREFIX)/include
    CXXFLAGS += -I$(SDL2_PREFIX)/include

    LDFLAGS += -L$(CURL_PREFIX)/lib
    LDFLAGS += -L$(TINYXML2_PREFIX)/lib
    LDFLAGS += -L$(SDL2_PREFIX)/lib

    LIBS += -lSDL2
else
    CXXFLAGS += -I../rpi-rgb-led-matrix
    CXXFLAGS += -I../rpi-rgb-led-matrix/include
    LDFLAGS += -L../rpi-rgb-led-matrix/lib
    LIBS += -lrgbmatrix -lrt
endif

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

clean:
	rm -rf $(OBJDIR) $(TARGET)

# Headless pixel/regression checks use the SDL-backed Mac matrix stub.
ifeq ($(UNAME), Darwin)
.PHONY: test
test:
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g tests/scrolling.cpp src/ESPNParser.cpp src/Display.cpp src/SportsDisplay.cpp src/Competition.cpp src/Team.cpp -o $(OBJDIR)/scrolling_test $(LDFLAGS) $(filter-out -lSDL2,$(LIBS))
	ASAN_OPTIONS=detect_leaks=0 ./$(OBJDIR)/scrolling_test
endif

ifeq ($(UNAME), Darwin)
.PHONY: test-weather
test-weather:
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -g tests/weather.cpp src/Display.cpp src/WeatherDisplay.cpp src/Weather.cpp src/SunStatus.cpp -o $(OBJDIR)/weather_test $(LDFLAGS) $(filter-out -lSDL2,$(LIBS))
	ASAN_OPTIONS=detect_leaks=0 ./$(OBJDIR)/weather_test
endif
