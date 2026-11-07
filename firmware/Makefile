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

$(BUILD)/vsi.elf: src/startup.S src/main.c src/color.c src/color.h link.ld | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) src/startup.S src/main.c src/color.c -lgcc -o $@

$(BUILD)/vsi.hex: $(BUILD)/vsi.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD):
	mkdir -p $@

test: | $(BUILD)
	gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g src/color.c test/test_color.c -o $(BUILD)/test_color
	$(BUILD)/test_color

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
