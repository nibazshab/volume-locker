CXX = x86_64-w64-mingw32-g++

CXXFLAGS = -static
LDFLAGS = -lole32 -loleaut32

TARGET = volume_locker.exe

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET)
