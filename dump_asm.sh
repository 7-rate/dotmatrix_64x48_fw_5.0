#!/bin/sh

~/.platformio/packages/toolchain-xtensa32/bin/xtensa-esp32-elf-objdump -S .pio/build/esp32dev/firmware.elf  > .pio/build/esp32dev/firmware.elf.txt
