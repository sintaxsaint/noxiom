# Noxiom OS - Build System
# Build environment: WSL2 (Ubuntu) on Windows
# Install deps: sudo apt install nasm gcc binutils qemu-system-x86 build-essential

ASM  := nasm
CC   := gcc
LD   := ld
QEMU := qemu-system-x86_64

CFLAGS := -std=c11 -ffreestanding -fno-stack-protector \
          -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
          -mcmodel=kernel -Wall -Wextra \
          -Ikernel/src

LDFLAGS := -T kernel/linker.ld -nostdlib -static -z max-page-size=0x1000

BUILD := build

# C sources (listed explicitly for clarity)
C_SRCS := \
    kernel/src/main.c       \
    kernel/src/vga.c        \
    kernel/src/serial.c     \
    kernel/src/gdt.c        \
    kernel/src/idt.c        \
    kernel/src/pic.c        \
    kernel/src/keyboard.c   \
    kernel/src/string.c     \
    kernel/src/shell/shell.c

# Derive object paths: kernel/src/foo.c -> build/foo.o
C_OBJS := $(patsubst kernel/src/%.c, $(BUILD)/%.o, $(C_SRCS))

# ─── Targets ───────────────────────────────────────────────────────

.PHONY: all run clean

all: noxiom.img

# Assemble final disk image
noxiom.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	dd if=/dev/zero           of=$@                 bs=512 count=4096
	dd if=$(BUILD)/stage1.bin of=$@ bs=512 seek=0  conv=notrunc
	dd if=$(BUILD)/stage2.bin of=$@ bs=512 seek=1  conv=notrunc
	dd if=$(BUILD)/kernel.bin of=$@ bs=512 seek=17 conv=notrunc

# Bootloader binaries
$(BUILD)/stage1.bin: boot/stage1.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

$(BUILD)/stage2.bin: boot/stage2.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

# Kernel entry (must come first in the link order)
$(BUILD)/entry.o: kernel/src/entry.asm | $(BUILD)
	$(ASM) -f elf64 $< -o $@

# Link kernel — entry.o first so _start is at 0x100000
$(BUILD)/kernel.bin: $(BUILD)/entry.o $(C_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

# C object files (handles subdirectories via mkdir -p)
$(BUILD)/%.o: kernel/src/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory tree
$(BUILD):
	mkdir -p $(BUILD)/shell $(BUILD)/mem

# ─── Run in QEMU ───────────────────────────────────────────────────

run: noxiom.img
	$(QEMU) \
	    -drive format=raw,file=$<,if=ide \
	    -serial stdio                     \
	    -m 128M                           \
	    -no-reboot                        \
	    -no-shutdown

# ─── Clean ─────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD) noxiom.img
