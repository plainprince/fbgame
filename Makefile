CXX=ccache g++
CXXFLAGS=-std=c++17 -O3 -mcpu=native -pipe -Wall -Wextra -Iinclude
LDLIBS=$(shell pkg-config --libs alsa 2>/dev/null || echo "-lasound") \
       -lpthread -flto -s

SRC=$(wildcard src/*pp)
OBJ=$(SRC:.cpp=.o)

.PHONY: all clean luajit lua5.4

all: lua5.4

luajit: CXXFLAGS += -I/usr/include/luajit-2.1
luajit: LDLIBS += -lluajit-5.1
luajit: app

lua5.4: CXXFLAGS += -I/usr/include/lua5.4
lua5.4: LDLIBS += -llua5.4
lua5.4: app

app: $(OBJ)
	$(CXX) $(OBJ) -o app $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) app
