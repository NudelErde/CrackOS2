cpp_files := $(shell find src -name *.cpp)
cpp_object_files := $(patsubst src/%.cpp, build/cpp_object_files/%.o, $(cpp_files))

asm_source_files := $(shell find src -name *.asm)
asm_object_files := $(patsubst src/%.asm, build/asm_object_files/%.o, $(asm_source_files))

object_files := $(asm_object_files) $(cpp_object_files)

$(cpp_object_files): build/cpp_object_files/%.o : src/%.cpp
	mkdir -p $(dir $@) && \
	x86_64-elf-g++ -c -fPIC -I header -std=c++17 -fno-asynchronous-unwind-tables -Wno-multichar -Wno-literal-suffix -fno-exceptions -fno-rtti -fno-common -mno-red-zone -mgeneral-regs-only -ffreestanding $(patsubst build/cpp_object_files/%.o, src/%.cpp, $@) -o $@

$(asm_object_files): build/asm_object_files/%.o : src/%.asm
	mkdir -p $(dir $@) && \
	nasm -f elf64 $(patsubst build/asm_object_files/%.o, src/%.asm, $@) -o $@

.PHONY: build-x86_64
build-x86_64: $(object_files)
	mkdir -p dist && \
	x86_64-elf-ld --no-relax -n -o dist/kernel.bin -T targets/linker.ld $(object_files) && \
	objdump -dC dist/kernel.bin > dist/kernel.asm && \
	cp dist/kernel.bin targets/iso/boot/kernel.bin && \
	grub-mkrescue -d /usr/lib/grub/i386-pc -o dist/kernel.iso targets/iso