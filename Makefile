BUILD = build
SRC = src

K210-SERIALPORT = /dev/ttyUSB0
K210-BURNER = platform/k210/kflash.py
BOOTLOADER = platform/k210/rustsbi-k210.bin
K210_BOOTLOADER_SIZE = 131072

KERNEL_BIN = k210.bin
KERNEL_O = $(BUILD)/kernel.o

GCC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy

all:
	$(GCC) -o $(KERNEL_O) -I src/lib -Wall -g -mcmodel=medany -T src/linker.ld -O2 -ffreestanding -nostdlib\
                                    src/asm/boot.s\
                                    src/asm/interrupt.s\
                                    src/lib/interrupt.c\
                                    src/lib/interrupt.h\
                                    src/lib/math.c\
                                    src/lib/math.h\
                                    src/lib/memory.c\
                                    src/lib/memory.h\
                                    src/lib/sbi.c\
                                    src/lib/sbi.h\
                                    src/lib/stdarg.h\
                                    src/lib/stdbool.h\
                                    src/lib/stddef.h\
                                    src/lib/stdio.c\
                                    src/lib/stdio.h\
                                    src/lib/stl.h\
                                    src/lib/stl.c\
                                    src/lib/page.c\
                                    src/lib/page.h\
                                    src/main.c
	$(OBJCOPY) $(KERNEL_O) --strip-all -O binary $(KERNEL_BIN)

run: all up see

up:
	@cp $(BOOTLOADER) $(BOOTLOADER).copy
	@dd if=$(KERNEL_BIN) of=$(BOOTLOADER).copy bs=$(K210_BOOTLOADER_SIZE) seek=1
	@mv $(BOOTLOADER).copy $(KERNEL_BIN)
	@sudo chmod 777 $(K210-SERIALPORT)
	python3 $(K210-BURNER) -p $(K210-SERIALPORT) -b 1500000 $(KERNEL_BIN)

see:
	python3 -m serial.tools.miniterm --eol LF --dtr 0 --rts 0 --filter direct $(K210-SERIALPORT) 115200

