import subprocess
import sys
import re
import os

def get_function_start_address(function_name):
    kernel_release = os.uname().release
    vmlinux_path = f"/usr/lib/debug/lib/modules/{kernel_release}/vmlinux"
    print(vmlinux_path)

    # execute GDB command
    cmd = [
        "gdb",
        "-batch",
        "-ex",
        f"info line {function_name}",
        vmlinux_path
    ]

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        print("GDB execution failed:")
        print(e.output)
        return None

    match = re.search(r"starts at address ([\w]+)", output)
    if match:
        return match.group(1)
    else:
        print("Failed to find start address.")
        return None

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: sudo python3 {sys.argv[0]} <function_name>")
        sys.exit(1)

    function = sys.argv[1]
    addr = get_function_start_address(function)
    if addr:
        print(f"{function} starts at address: {addr}")