"""
Lists the biggest variables in the game's memory (the .bss: zeroed globals and statics), largest
first, with the object file each is in. The SH toolchain here has no nm, and the link map
(game.map) names only global symbols: statics get counted into the global before them. This
reads each object file's symbol table itself (ELF, 32-bit, big-endian: the SH-2).

  python tools/bss_sizes.py [how many, default 40]      (after a build: the .o files)

Work RAM's limit: the .bss must end below 0x060C0000 (game.map: .bss's address + size).
"""
import glob
import os
import struct
import sys

here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def symbols(path):
    data = open(path, 'rb').read()
    if data[:4] != b'\x7fELF':
        return []
    e_shoff, = struct.unpack_from('>I', data, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('>HHH', data, 0x2E)
    sections = []
    for i in range(e_shnum):
        sections.append(struct.unpack_from('>IIIIIIIIII', data, e_shoff + i * e_shentsize))
    names_off = sections[e_shstrndx][4]

    def section_name(sh):
        start = names_off + sh[0]
        return data[start:data.index(b'\0', start)].decode()

    bss = {i for i, sh in enumerate(sections) if section_name(sh).startswith('.bss')}
    out = []
    for sh in sections:
        if sh[1] != 2:  # SHT_SYMTAB
            continue
        strtab = sections[sh[6]][4]
        for k in range(sh[5] // 16):
            st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from('>IIIBBH', data, sh[4] + k * 16)
            if st_size == 0 or (st_shndx not in bss and st_shndx != 0xFFF2):  # .bss, or COMMON
                continue
            name = data[strtab + st_name:data.index(b'\0', strtab + st_name)].decode()
            out.append((st_size, name, os.path.relpath(path, here)))
    return out


found = []
for pattern in ('*.o', 'src/*.o', 'src/game/*.o', 'src/utils/*.o', '../../jo_engine/*.o'):
    for path in glob.glob(os.path.join(here, pattern)):
        found += symbols(path)
found.sort(reverse=True)
count = int(sys.argv[1]) if len(sys.argv) > 1 else 40
for size, name, path in found[:count]:
    print(f'{size:8d}  {name:36s} {path}')
print(f'{sum(s for s, _, _ in found):8d}  in all ({len(found)} variables)')
