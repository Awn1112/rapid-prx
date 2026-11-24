TARGET  := rapid
CFLAGS  := -O2 -Wall -fPIC -std=gnu11 -I$(OO_PS4_TOOLCHAIN)/include
LDFLAGS := -L$(OO_PS4_TOOLCHAIN)/lib -lSceVideoOut -lScePad -lSceLibcInternal -lkernel

$(TARGET).prx: $(TARGET).o
	$(CC) $(TARGET).o $(LDFLAGS) -nostdlib -Wl,--entry=module_start -Wl,-soname=$(TARGET).prx -o $(TARGET).elf
	$(OO_PS4_TOOLCHAIN)/bin/create-fself -in=$(TARGET).elf -out=$(TARGET).oelf -lib=$(TARGET).prx

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf *.oelf *.prx
