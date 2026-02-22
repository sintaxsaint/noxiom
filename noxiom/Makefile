# noxiom/Makefile — arch-dispatching inner Makefile
# Included by the top-level Makefile; invoked as: make -C noxiom ARCH=<arch>

ARCH ?= x86_64

ifeq ($(ARCH),x86_64)
    include arch/x86_64/rules.mk
else ifeq ($(ARCH),arm64)
    include arch/arm64/rules.mk
else
    $(error Unsupported ARCH=$(ARCH). Use: ARCH=x86_64 or ARCH=arm64)
endif
