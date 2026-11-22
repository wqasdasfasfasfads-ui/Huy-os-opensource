# Huy OS

Власна 64-бітна хобі-операційна система, написана з нуля: свій завантажувач (`boot.asm`) + графічне ядро на C (`kernel.c`). Без Linux, без Windows, без libc.

Вигляд Windows 11, розкладка macOS (верхнє меню + док).

> Це навчальний/експериментальний проєкт. Він запускається у віртуальній машині, **не** на реальному залізі з важливими даними.

## Можливості

- 64-бітний long mode, Legacy BIOS (не UEFI), мишка на перериваннях (IRQ12)
- Вікна, перетягування, Start-меню, нижня панель, права кнопка, подвійний клік
- **Files** (папки, створення/перейменування файлів), **Settings**, **Terminal**, текстовий редактор, годинник
- **Huygle**: пошук у стилі Google (програми, файли, довідка, калькулятор `12*7`, time/date)
- **Huy AI** у меню Start
- 3D на цілих числах (без FPU): 3D Viewer, 3D Space, 3D Maze
- Запуск `.bat`, `.sh`, Linux ELF64 (статичні, syscall `write`/`exit`) та Windows PE64 `.exe` (підмножина `kernel32`/`msvcrt`)
- Образи: `huyos.img` (RAW диск) та `huyos.iso` (El Torito), + `.vdi` / `.vmdk`

## Збірка

Linux / WSL:

```bash
sudo apt install nasm gcc binutils python3 qemu-utils qemu-system-x86
./build.sh        # або: make
```

Результат у `build/`: `huyos.img`, `huyos.iso`, `huyos.vdi`, `huyos.vmdk`.

## Запуск

```bash
# QEMU, з диска
qemu-system-x86_64 -m 128 -drive format=raw,file=build/huyos.img -vga std
# QEMU, з ISO
qemu-system-x86_64 -m 128 -cdrom build/huyos.iso -boot d -vga std
# або просто
make run
```

**VirtualBox:** Type `Other`, Version `Other/Unknown (64-bit)`, RAM 128+ МБ, Video Memory 16+ МБ, **зняти Enable EFI**, Pointing Device = **PS/2 Mouse** (не USB Tablet). Підключіть `huyos.iso` до порожнього IDE-приводу. Детальніше: [docs/USAGE.md](docs/USAGE.md).

## Структура

```
src/        boot.asm, entry.asm, kernel.c, linker.ld
files/      файли, що вбудовуються в образ ОС при збірці
examples/   вихідники демо-програм (hello.exe, hello ELF) на NASM
tools/      mkiso.py - генератор ISO без xorriso
build.sh    збірка
docs/       документація
```

Свої файли кладіть у `files/` - вони потрапляють у файлову систему ОС після `./build.sh`.

## Обмеження

- `.bat`/`.sh`: прості команди (`echo`, `set`, змінні, виклик команд Terminal); без `if`/`for`/`goto`
- ELF: лише статичні, без libc. EXE: лише PE64 з імпортами `kernel32` і `msvcrt` (див. вище)
- Усе працює в ring 0, без ізоляції та багатозадачності
- Файли живуть у RAM; диска/FAT поки немає

## Roadmap

- FAT32 / ext2
- Більше syscalls Linux і Win32 API
- Mach-O loader
- Мережа, USB, багатозадачність

## Як долучитися

Форки та pull request-и вітаються. Див. [CONTRIBUTING.md](CONTRIBUTING.md).

## Ліцензія

[MIT](LICENSE): робіть з кодом що завгодно, форкайте, змінюйте, поширюйте, зокрема комерційно, залишаючи повідомлення про авторство.
