TARGET = espn_jsonc
SRCDIR = src
OBJDIR = build
SRC = $(wildcard $(SRCDIR)/*.cpp)
OBJ = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRC))

CXX = g++
CXXFLAGS = -Wall -O2 $(shell pkg-config --cflags Magick++) \
           -I../rpi-rgb-led-matrix -I../rpi-rgb-led-matrix/include

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
    JSONC_PREFIX := $(shell brew --prefix json-c)
    CURL_PREFIX := $(shell brew --prefix curl)
    CXXFLAGS += -I$(JSONC_PREFIX)/include -I$(CURL_PREFIX)/include
    LDFLAGS += -L$(JSONC_PREFIX)/lib -L$(CURL_PREFIX)/lib
else
    LDFLAGS += -L../rpi-rgb-led-matrix/lib
endif

LIBS = -ljson-c -lcurl -ltinyxml2 -lrgbmatrix -lrt -lm -lpthread $(shell pkg-config --libs Magick++)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
