# Noxiom OS — Top-Level Makefile
# Build environment: WSL2 (Ubuntu) on Windows
#
# Usage:
#   make                  → build x86_64 image (default)
#   make ARCH=arm64       → build arm64 kernel8.img (Raspberry Pi)
#   make run              → build x86_64 + launch QEMU
#   make run ARCH=arm64   → build arm64 + launch QEMU (raspi3b)
#   make dist             → build both arches → imgs/noxiom-x86_64.img
#                                               imgs/noxiom-arm64.img
#   make clean            → clean all build output
#
# Install deps (WSL2):
#   sudo apt install nasm gcc binutils qemu-system-x86 qemu-system-arm \
#                    gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
#                    build-essential

ARCH ?= x86_64

.PHONY: all run dist clean

all:
	$(MAKE) -C noxiom ARCH=$(ARCH)

run:
	$(MAKE) -C noxiom ARCH=$(ARCH) run

# Build both architectures and copy release images to imgs/
dist:
	$(MAKE) -C noxiom ARCH=x86_64
	$(MAKE) -C noxiom ARCH=arm64
	@mkdir -p imgs
	cp noxiom/build/x86_64/noxiom.img   imgs/noxiom-x86_64.img
	cp noxiom/build/arm64/kernel8.img    imgs/noxiom-arm64.img
	@echo ""
	@echo "Release images ready in imgs/:"
	@ls -lh imgs/*.img

clean:
	$(MAKE) -C noxiom ARCH=x86_64 clean
	-$(MAKE) -C noxiom ARCH=arm64 clean
