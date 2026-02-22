# arch/arm64/rules.mk — AArch64 cross-compile build rules
# Included by noxiom/Makefile when ARCH=arm64
# All paths are relative to noxiom/ (where make is invoked)
#
# Prerequisites (WSL2/Ubuntu):
#   sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
#                    qemu-system-arm

CC      := aarch64-linux-gnu-gcc
AS      := aarch64-linux-gnu-as
LD      := aarch64-linux-gnu-ld
OBJCOPY := aarch64-linux-gnu-objcopy

BUILD   := build/arm64

CFLAGS  := -std=c11 -ffreestanding -fno-pie -fno-pic \
           -fno-stack-protector -march=armv8-a -O2    \
           -Wall -Wextra                               \
           -Ikernel/src -Iarch/arm64

ASFLAGS := -march=armv8-a

# ── Sources ─────────────────────────────────────────────────────────────────
KERNEL_SRCS := \
    kernel/src/main.c          \
    kernel/src/hal_hw_detect.c \
    kernel/src/string.c        \
    kernel/src/shell/shell.c

ARCH_SRCS := \
    arch/arm64/hal_impl.c      \
    arch/arm64/uart_pl011.c    \
    arch/arm64/gic.c           \
    arch/arm64/dtb.c           \
    arch/arm64/midr.c

C_SRCS := $(KERNEL_SRCS) $(ARCH_SRCS)
C_OBJS := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))

S_SRCS := \
    arch/arm64/boot/entry.S    \
    arch/arm64/exceptions.S

S_OBJS := $(patsubst %.S, $(BUILD)/%.o, $(S_SRCS))

# Entry object must be first so the linker places it at 0x80000
OBJS := $(BUILD)/arch/arm64/boot/entry.o \
        $(filter-out $(BUILD)/arch/arm64/boot/entry.o, $(S_OBJS) $(C_OBJS))

KERNEL_ELF := $(BUILD)/kernel.elf
KERNEL_IMG := $(BUILD)/kernel8.img

.PHONY: all run clean

all: $(KERNEL_IMG)

# ── Final binary: strip ELF → raw binary ────────────────────────────────────
$(KERNEL_IMG): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

# ── Link ────────────────────────────────────────────────────────────────────
$(KERNEL_ELF): $(OBJS)
	$(LD) -T arch/arm64/linker.ld -o $@ $^

# ── Compile C ───────────────────────────────────────────────────────────────
$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Assemble .S (run through GCC preprocessor for #include / .equ) ──────────
$(BUILD)/%.o: %.S | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

# ── Build directory ─────────────────────────────────────────────────────────
$(BUILD):
	mkdir -p $(BUILD)

# ── Run in QEMU (raspi3b machine; built-in DTB covers PL011 + GIC) ──────────
run: $(KERNEL_IMG)
	qemu-system-aarch64        \
	    -M raspi3b             \
	    -kernel $(KERNEL_IMG)  \
	    -serial stdio          \
	    -display none          \
	    -m 1G

clean:
	rm -rf $(BUILD)
