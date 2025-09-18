#!/usr/bin/env python3
from elftools.elf.elffile import ELFFile
import sys

def find_function_address(vmlinux_path, func_name):
    with open(vmlinux_path, 'rb') as f:
        elf = ELFFile(f)

        symtab = None
        if elf.get_section_by_name('.symtab'):
            symtab = elf.get_section_by_name('.symtab')
        elif elf.get_section_by_name('.dynsym'):
            symtab = elf.get_section_by_name('.dynsym')
        else:
            print("No symbol table found in vmlinux")
            return

        for sym in symtab.iter_symbols():
            if sym.name == func_name and sym['st_info']['type'] == 'STT_FUNC':
                print(f"Found: {func_name} @ 0x{sym['st_value']:x}")
                return sym['st_value']

        print(f"Function {func_name} not found.")
        return None

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: ./find_symbol_addr.py <function name>")
        sys.exit(1)

    import os
    kernel_release = os.uname().release
    vmlinux_path = f"/usr/lib/debug/lib/modules/{kernel_release}/vmlinux"
    func_name = sys.argv[1]
    find_function_address(vmlinux_path, func_name)
