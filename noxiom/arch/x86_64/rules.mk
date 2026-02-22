# arch/x86_64/rules.mk — build rules for x86_64 target
# Included by noxiom/Makefile when ARCH=x86_64
# All paths are relative to noxiom/ (where make is invoked)

ASM  := nasm
CC   := gcc
LD   := ld
QEMU := qemu-system-x86_64

BUILD := build/x86_64

CFLAGS := -std=c11 -ffreestanding -fno-stack-protector \
          -fno-pic -fno-pie \
          -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone \
          -mcmodel=kernel -Wall -Wextra \
          -Ikernel/src -Iarch/x86_64

LDFLAGS := -T arch/x86_64/linker.ld -nostdlib -static -z max-page-size=0x1000

# Portable kernel sources
KERNEL_SRCS := \
    kernel/src/main.c           \
    kernel/src/hal_hw_detect.c  \
    kernel/src/string.c         \
    kernel/src/shell/shell.c

# x86_64-specific sources
ARCH_SRCS := \
    arch/x86_64/hal_impl.c      \
    arch/x86_64/vga.c           \
    arch/x86_64/serial_x86.c    \
    arch/x86_64/keyboard_x86.c  \
    arch/x86_64/gdt.c           \
    arch/x86_64/idt.c           \
    arch/x86_64/pic.c           \
    arch/x86_64/cpuid.c

C_SRCS := $(KERNEL_SRCS) $(ARCH_SRCS)
C_OBJS := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))

.PHONY: all run clean

all: $(BUILD)/noxiom.img

# ── Disk image ──────────────────────────────────────────────────────────────
$(BUILD)/noxiom.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	dd if=/dev/zero           of=$@                 bs=512 count=4096
	dd if=$(BUILD)/stage1.bin of=$@ bs=512 seek=0  conv=notrunc
	dd if=$(BUILD)/stage2.bin of=$@ bs=512 seek=1  conv=notrunc
	dd if=$(BUILD)/kernel.bin of=$@ bs=512 seek=17 conv=notrunc

# ── Bootloader ─────────────────────────────────────────────────────────────
$(BUILD)/stage1.bin: arch/x86_64/boot/stage1.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

$(BUILD)/stage2.bin: arch/x86_64/boot/stage2.asm | $(BUILD)
	$(ASM) -f bin $< -o $@

# ── Kernel entry (must be first in link order so _start = 0x100000) ────────
$(BUILD)/entry.o: arch/x86_64/entry.asm | $(BUILD)
	$(ASM) -f elf64 $< -o $@

# ── Kernel binary ──────────────────────────────────────────────────────────
$(BUILD)/kernel.bin: $(BUILD)/entry.o $(C_OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

# ── C object files (preserves directory structure under BUILD) ─────────────
$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Build directory ────────────────────────────────────────────────────────
$(BUILD):
	mkdir -p $(BUILD)

# ── Run in QEMU ────────────────────────────────────────────────────────────
run: $(BUILD)/noxiom.img
	$(QEMU) \
	    -drive format=raw,file=$<,if=ide \
	    -serial stdio                     \
	    -m 128M                           \
	    -no-reboot                        \
	    -no-shutdown

clean:
	rm -rf $(BUILD)
