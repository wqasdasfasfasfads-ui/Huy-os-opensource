<!-- Оригінальні нотатки автора (українською): інструкції запуску у VirtualBox/VMware/QEMU та історія версій -->
```text
Huy OS v0.4 (64-bit)
====================
НОВЕ (huygle): програма Huygle (меню Start або "search" у Huy AI) - пошук у стилі Google: програми, файли (імена і текст), довідка, калькулятор 12*7, time/date.
НОВЕ в 0.4: huyos.iso (VirtualBox: порожній оптичний привід -> huyos.iso, EFI вимкнути); запуск .bat, .sh, Linux ELF64 і Windows PE64 .exe.
Файли беруться з папки files/ (вбудовуються в ядро при ./build.sh, потрібен python3). Запуск: набрати ім'я файлу в Terminal, `run <файл>`, або кнопка Run у Files.
Обмеження: .bat/.sh - прості команди (echo, set, змінні, виклик команд Terminal і інших файлів; без if/for/goto). ELF - лише статичні без libc (syscall write/exit). EXE - лише 64-бітні з імпортами kernel32 (GetStdHandle, WriteFile, WriteConsoleA, ExitProcess) і msvcrt (puts, putchar, exit). Програми працюють без ізоляції (ring 0), без багатозадачності.
НОВЕ в 0.3: 64-бітний long mode, мишка на перериваннях (IRQ12), екран завантаження, іконка-круг "Хуй", Huy AI в меню Start.
В VirtualBox: Type "Other", Version "Other/Unknown (64-bit)", Pointing Device = PS/2 Mouse (НЕ USB Tablet!), без EFI.
Диск: VBoxManage convertfromraw huyos.img huyos.vdi --format VDI
QEMU: qemu-system-x86_64 -m 128 -drive format=raw,file=huyos.img -vga std

Власна ОС: свій завантажувач (boot.asm) + ядро на C (kernel.c). Без Linux і Windows.
Вигляд Windows 11, розкладка macOS (верхнє меню + док).

ISO (huyos.iso) - найпростіший спосіб
-------------------------------------
VirtualBox: New > Type "Other", Version "Other/Unknown" > RAM 128+ МБ > "Do not add a virtual hard disk".
  Settings > System: зняти "Enable EFI", Pointing Device = PS/2 Mouse.
  Settings > Storage: Controller IDE > порожній привід > Choose a disk file > huyos.iso.
  Settings > Display: Video Memory 16+ МБ. Start.
  Команда: VBoxManage storageattach HuyOS --storagectl IDE --port 1 --device 0 --type dvddrive --medium huyos.iso
QEMU:    qemu-system-x86_64 -m 128 -cdrom huyos.iso -boot d -vga std
VMware:  New VM > Installer disc image file (iso) > Other / Other 32-bit.
Тільки Legacy BIOS (не UEFI). Диск не потрібен - система працює з ISO.

VirtualBox з диском .vdi (альтернатива)
-------------------
1. Machine > New. Name: Huy OS. Type: Other, Version: Other/Unknown (32-bit). RAM: 128 МБ або більше.
2. "Hard Disk": Use an existing virtual hard disk file -> вибери huyos.vdi.
3. Settings > System > Motherboard:
   - ЗНЯТИ галочку "Enable EFI" (потрібен класичний BIOS!)
   - Pointing Device: PS/2 Mouse   <-- важливо, USB Tablet не підтримується
4. Settings > Display: Video Memory 16 МБ або більше. Graphics Controller: VBoxVGA або VBoxSVGA.
5. Start. Мишка "захоплюється" кліком у вікні; звільнити - правий Ctrl (Host key).

Командний рядок (те саме):
  VBoxManage createvm --name HuyOS --ostype Other --register
  VBoxManage modifyvm HuyOS --memory 128 --vram 16 --firmware bios --mouse ps2 --graphicscontroller vboxvga
  VBoxManage storagectl HuyOS --name IDE --add ide
  VBoxManage storageattach HuyOS --storagectl IDE --port 0 --device 0 --type hdd --medium huyos.vdi
  VBoxManage startvm HuyOS

VMware: huyos.vmdk.   QEMU: qemu-system-x86_64 -m 128 -drive format=raw,file=huyos.img -vga std
Збірка (Linux/WSL): sudo apt install nasm gcc binutils qemu-utils && ./build.sh

Що вміє: мишка, вікна, Start, Files, Settings, Terminal (help, ls, info <файл>, calc ...), годинник.
Файли: розпізнає Windows PE (.exe), Linux ELF, macOS Mach-O. ЗАПУСКАТИ їх поки не вміє.
Файли в RAM (демо), диска/FAT ще немає.

НОВЕ (desktop): перетягування вікон за заголовок; права кнопка миші на робочому столі та у Files -> New folder / New text file (папки можна відкривати, кнопка Up повертає назад; у Files є розділ Desktop);
нижня панель з кнопками відкритих вікон (клік: показати / згорнути); клавіатурна миша: стрілки = рух, F2 = ліва кнопка (тримай + стрілки = тягнути), F1 = права кнопка.
НОВЕ (desktop2): права кнопка по файлу (на столі або у Files) -> Open / Edit (для .txt .bat .sh) / Rename. Edit відкриває текстовий редактор (клік ставить курсор, кнопка Save зберігає; збережений .bat можна запускати). Rename дозволяє змінити ім'я й розширення (.txt -> .bat тощо). Іконки файлів у стилі Windows.
НОВЕ (desktop3): подвійний клік як у Windows (на столі і у Files): папка відкривається, .txt - у редакторі, .bat/.sh/.exe/ELF запускаються в Terminal, решта - File Inspector. Один клік тільки виділяє.
НОВЕ (desktop4): 3D-графіка на цілих числах (без FPU) і три програми з меню у Start: 3D Viewer (куб, піраміда, тор, сфера; Solid/Wire; Spin; обертання мишкою), 3D Space (політ крізь зорі), 3D Maze (3D-лабіринт, W/S/A/D, мінікарта). Збірка з -Os (ядро менше).
```
