#!/usr/bin/env python3
"""Run host boundary tests and a real Cortex-M3 context-switch test in QEMU."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "tests" / ".build"


def run(command, **kwargs):
    subprocess.run([str(part) for part in command], cwd=ROOT, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-only", action="store_true")
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    host = shlex.split(os.environ.get("CC", "cc"))
    flags = shlex.split(os.environ.get("HOST_CFLAGS", ""))
    run(host + flags + ["-std=c11", "-Wall", "-Wextra", "-Werror",
                       "-fsanitize=undefined", "-g", "-Isrc",
                       "-Iboards/nucleo-f103rb", "tests/kernel_test.c",
                       "-o", BUILD / "kernel_test"])
    run([BUILD / "kernel_test"])
    if args.host_only:
        return
    for tool in ["arm-none-eabi-gcc", "qemu-system-arm"]:
        if shutil.which(tool) is None:
            raise SystemExit(f"Missing {tool}; install it or use --host-only")
    linker = (ROOT / "boards/nucleo-f103rb/linker.ld").read_text()
    (BUILD / "test.ld").write_text(linker.replace("0x08000000", "0x00000000"))
    sources = ["tests/cortex_m3_test.c", "tests/registers.s",
               "src/kernel/task.c", "src/kernel/scheduler.c", "src/kernel/sync.c",
               "src/kernel/debug.c", "src/port/cortex_m3/port.c",
               "src/port/cortex_m3/context.s", "boards/nucleo-f103rb/startup.s"]
    for optimization in ["-O0", "-O2"]:
        elf = BUILD / f"cortex_m3_{optimization[1:]}.elf"
        run(["arm-none-eabi-gcc", "-mcpu=cortex-m3", "-mthumb", "-std=c11",
             optimization, "-g", "-Wall", "-Wextra", "-Werror", "-ffreestanding",
             "-fno-builtin", "-fno-common", "-nostdlib", "-Isrc",
             "-Iboards/nucleo-f103rb", *sources, "-Wl,--build-id=none",
             f"-Wl,-T,{BUILD / 'test.ld'}", "-o", elf])
        print(f"QEMU {optimization}", flush=True)
        run(["qemu-system-arm", "-M", "mps2-an385", "-nographic", "-monitor",
             "none", "-serial", "none", "-semihosting-config", "enable=on,target=native",
             "-kernel", elf], timeout=15)


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"Test failed: {error}", file=sys.stderr)
        sys.exit(1)
