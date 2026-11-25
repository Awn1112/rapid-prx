TARGET  := rapid
OO      ?= $(OO_PS4_TOOLCHAIN)

CREATE_FSELF := $(firstword $(wildcard $(OO)/bin/create-fself) $(wildcard $(OO)/bin/linux/create-fself) $(shell find "$(OO)" -type f -name create-fself -print -quit 2>/dev/null))
ifeq ($(strip $(CREATE_FSELF)),)
$(error create-fself not found under $(OO))
endif

CC      := clang
CFLAGS  := -O2 -Wall -fPIC -std=gnu11 -I$(OO)/include
LDFLAGS := -L$(OO)/lib -lSceVideoOut -lScePad -lSceLibcInternal -lkernel

$(TARGET).prx: $(TARGET).o
	$(CC) $(TARGET).o $(LDFLAGS) -nostdlib -Wl,--entry=module_start -Wl,-soname=$(TARGET).prx -o $(TARGET).elf
	$(CREATE_FSELF) -in=$(TARGET).elf -out=$(TARGET).oelf -lib=$(TARGET).prx

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.oelf *.prx
