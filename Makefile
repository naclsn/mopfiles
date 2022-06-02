ASFLAGS = -f elf64
LDFLAGS = -m elf_x86_64

all: release
clean:
	rm -rf ./bin

DEPS = $(filter %.asm,$(shell nasm -M uh.asm))

release: bin $(DEPS)
	nasm $(ASFLAGS) -o 'bin/uh.o' 'uh.asm'
	ld $(LDFLAGS) -o 'bin/uh' 'bin/uh.o' -s

debug: bin $(DEPS)
	nasm $(ASFLAGS) -o 'bin/uh.o' 'uh.asm' -F dwarf -g -l 'bin/uh.lst'
	ld $(LDFLAGS) -o 'bin/uh' 'bin/uh.o'

bin:
	mkdir ./bin
