CXX=ccache g++
CXXFLAGS=-std=c++17 -O3 -mcpu=native -pipe -Wall -Wextra -Iinclude \
	$(shell pkg-config --cflags lua5.4 2>/dev/null || echo "-I/usr/include/lua5.4")
LDFLAGS=$(shell pkg-config --libs lua5.4 alsa 2>/dev/null || echo "-llua5.4 -lasound") \
	-lpthread -lstdc++fs -flto -s

SRC=$(wildcard src/*pp)
OBJ=$(SRC:.cpp=.o)

app: $(OBJ)
	$(CXX) $(OBJ) -o app $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) app

.PHONY: clean
