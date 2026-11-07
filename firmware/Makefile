# riscv-none-elf-gcc (xPack / MounRiver 付属) を想定。別のツールチェーンは CROSS で指定する。
CROSS  ?= riscv-none-elf-
CC     := $(CROSS)gcc
OBJCOPY:= $(CROSS)objcopy
SIZE   := $(CROSS)size
BUILD  := build

ARCHFLAGS ?= -march=rv32imac_zicsr -mabi=ilp32
CFLAGS := $(ARCHFLAGS) -Os -g -std=c11 -Wall -Wextra -Werror -ffreestanding \
          -ffunction-sections -fdata-sections -fno-common $(EXTRA_CFLAGS)
LDFLAGS := $(ARCHFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-T,link.ld \
           -Wl,-Map=$(BUILD)/vsi.map -Wl,--no-warn-rwx-segments

all: $(BUILD)/vsi.hex
	$(SIZE) $(BUILD)/vsi.elf

SRCS := src/startup.S src/main.c src/color.c src/dbglog.c

$(BUILD)/vsi.elf: $(SRCS) src/color.h src/dbglog.h link.ld | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -lgcc -o $@

$(BUILD)/vsi.hex: $(BUILD)/vsi.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD):
	mkdir -p $@

test: | $(BUILD)
	gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g src/color.c test/test_color.c -o $(BUILD)/test_color
	$(BUILD)/test_color
	gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g src/dbglog.c test/test_dbglog.c -o $(BUILD)/test_dbglog
	$(BUILD)/test_dbglog

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
