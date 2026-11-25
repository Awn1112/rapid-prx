TARGET    := rapid.prx
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
INTDIR    := build

CREATE_FSELF := $(firstword $(wildcard $(TOOLCHAIN)/bin/create-fself) $(wildcard $(TOOLCHAIN)/bin/linux/create-fself))

LIBS    := -lc -lkernel -lSceVideoOut -lScePad
CFLAGS  := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
LDFLAGS := -m elf_x86_64 -pie --allow-multiple-definition --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crtlib.o

CC := clang
LD := ld.lld

_unused := $(shell mkdir -p $(INTDIR))

$(TARGET): $(INTDIR)/rapid.o
	$(LD) $(INTDIR)/rapid.o -o $(INTDIR)/rapid.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(INTDIR)/rapid.elf -out=$(INTDIR)/rapid.oelf --lib=$(TARGET) --paid 0x3800000000000011

$(INTDIR)/rapid.o: rapid.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(INTDIR) $(TARGET)
