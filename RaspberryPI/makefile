TARGET = espn_jsonc

SRCDIR = src
OBJDIR = build

SRC = $(wildcard $(SRCDIR)/*.cpp)
OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRC))

UNAME := $(shell uname)

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17
LDFLAGS =
LIBS = -ljson-c -lcurl -ltinyxml2 -lm -lpthread

CXXFLAGS += $(shell pkg-config --cflags Magick++)
LIBS += $(shell pkg-config --libs Magick++)

ifeq ($(UNAME), Darwin)
    JSONC_PREFIX := $(shell brew --prefix json-c)
    CURL_PREFIX := $(shell brew --prefix curl)
    TINYXML2_PREFIX := $(shell brew --prefix tinyxml2)
    SDL2_PREFIX := $(shell brew --prefix sdl2)

    CXXFLAGS += -DMAC_STUB_MATRIX
    CXXFLAGS += -Imac_stub
    CXXFLAGS += -I$(JSONC_PREFIX)/include
    CXXFLAGS += -I$(CURL_PREFIX)/include
    CXXFLAGS += -I$(TINYXML2_PREFIX)/include
    CXXFLAGS += -I$(SDL2_PREFIX)/include

    LDFLAGS += -L$(JSONC_PREFIX)/lib
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
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)
