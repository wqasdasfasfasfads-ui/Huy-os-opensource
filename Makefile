.PHONY: all run run-iso clean
all:
	./build.sh
run: all
	qemu-system-x86_64 -m 128 -drive format=raw,file=build/huyos.img -vga std
run-iso: all
	qemu-system-x86_64 -m 128 -cdrom build/huyos.iso -boot d -vga std
clean:
	rm -rf build
