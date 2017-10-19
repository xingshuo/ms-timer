CFLAGS = -g -O0 -Wall -std=gnu99
SHARED := -fPIC --shared
INC = include
SRC = src
BUILD = build

all: $(BUILD)/mstimer.so

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/mstimer.so: $(SRC)/lua-timer.c $(SRC)/timer.c | $(BUILD)
	gcc $(CFLAGS) $(SHARED) $^ -o $@ -I$(INC) -lpthread

run:
	bin/lua test.lua

clean:
	rm $(BUILD)/*
