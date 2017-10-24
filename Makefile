CFLAGS = -g -O0 -Wall -std=gnu99
INC = include
SRC = src
BUILD = build

none:
	@echo "Please do 'make linux' or 'make mac'"

linux: SHARED = -fPIC --shared
mac: SHARED = -fPIC -dynamiclib -Wl,-undefined,dynamic_lookup

linux mac:
	$(MAKE) all SHARED="$(SHARED)"

all: $(BUILD)/mstimer.so

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/mstimer.so: $(SRC)/lua-timer.c $(SRC)/timer.c | $(BUILD)
	gcc $(CFLAGS) $(SHARED) $^ -o $@ -I$(INC) -lpthread

clean:
	rm -rf $(BUILD)
