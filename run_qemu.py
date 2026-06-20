#!/usr/bin/env python3
"""Generic xv6 QEMU test driver.

Usage:
  run_qemu.py <command> [command2 ...] [-- <extra-qemu-args>]

Boots xv6 in QEMU with the console wired to a pair of FIFOs, sends each
command followed by a newline, then halts.  All console output is streamed
to stdout and also returned in the script's stdout (already done via print).
Exit status 0 on clean exit, non-zero on kernel panic or unexpected.
"""

import os
import select
import subprocess
import sys
import time
import argparse


REPO = '/home/tfc/OS/OS_xv6_riscv'
IN_PIPE  = '/tmp/xv6_pipe.in'
OUT_PIPE = '/tmp/xv6_pipe.out'


def cleanup_pipes():
    for p in (IN_PIPE, OUT_PIPE):
        try:
            os.unlink(p)
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('commands', nargs='+', help='commands to send to xv6 shell')
    ap.add_argument('--timeout', type=int, default=20, help='per-command wait time (s)')
    ap.add_argument('--no-halt', action='store_true', help="don't send 'halt' at the end")
    args = ap.parse_args()

    # kill any stale QEMU (use -x with exact name; pkill -f matches our own shell)
    subprocess.run(['pkill', '-9', '-x', 'qemu-system-ris'], capture_output=True)
    time.sleep(1.0)
    cleanup_pipes()

    os.mkfifo(IN_PIPE)
    os.mkfifo(OUT_PIPE)

    qemu_args = [
        'qemu-system-riscv64',
        '-machine', 'virt', '-bios', 'none',
        '-kernel', 'build/kernel/kernel',
        '-m', '128M', '-smp', '1',
        '-nographic', '-display', 'none',
        '-global', 'virtio-mmio.force-legacy=false',
        '-drive', 'file=build/fs.img,if=none,format=raw,id=x0',
        '-drive', 'file=build/fs1.img,if=none,format=raw,id=x1',
        '-device', 'virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0',
        '-device', 'virtio-blk-device,drive=x1,bus=virtio-mmio-bus.1',
        '-device', 'virtio-net-device,netdev=net0,bus=virtio-mmio-bus.2',
        '-netdev', 'user,id=net0',
        '-serial', f'pipe:{IN_PIPE[:-3]}',  # /tmp/xv6_pipe
    ]
    proc = subprocess.Popen(qemu_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    print(f"QEMU PID: {proc.pid}", flush=True)

    captured = []
    def read_all(fd, duration):
        end = time.time() + duration
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                try:
                    data = os.read(fd, 4096)
                    if data:
                        text = data.decode('utf-8', errors='replace')
                        captured.append(text)
                        sys.stdout.write(text)
                        sys.stdout.flush()
                except (BlockingIOError, OSError):
                    break

    out_fd = os.open(OUT_PIPE, os.O_RDONLY | os.O_NONBLOCK)
    # read boot output; capture both pipe output and QEMU's stderr
    boot_buf = []
    def read_boot(fd, duration):
        end = time.time() + duration
        while time.time() < end:
            r, _, _ = select.select([fd, proc.stderr], [], [], 0.2)
            for src in r:
                if src is proc.stderr:
                    try:
                        line = proc.stderr.readline()
                        if line:
                            text = line.decode('utf-8', errors='replace')
                            boot_buf.append(('STDERR', text))
                            print(f'[qemu] {text}', end='', flush=True)
                    except Exception:
                        pass
                else:
                    try:
                        data = os.read(fd, 4096)
                        if data:
                            text = data.decode('utf-8', errors='replace')
                            boot_buf.append(('STDOUT', text))
                            sys.stdout.write(text)
                            sys.stdout.flush()
                    except (BlockingIOError, OSError):
                        pass
            # check if QEMU is still alive
            if proc.poll() is not None:
                print(f'QEMU exited with code {proc.returncode} during boot', flush=True)
                break

    read_boot(out_fd, 8)
    full = ''.join(t for _, t in boot_buf)
    if proc.poll() is not None:
        print('--- QEMU terminated before shell ready ---', flush=True)
        cleanup_pipes()
        return 1

    in_fd = os.open(IN_PIPE, os.O_WRONLY | os.O_NONBLOCK)
    for cmd in args.commands:
        os.write(in_fd, (cmd + '\n').encode())
        read_all(out_fd, args.timeout)

    if not args.no_halt:
        try:
            os.write(in_fd, b'halt\n')
        except OSError:
            pass
        read_all(out_fd, 2)

    os.close(in_fd)
    os.close(out_fd)
    try:
        out, err = proc.communicate(timeout=5)
        if out:
            print('--- QEMU STDOUT ---')
            print(out.decode('utf-8', errors='replace'))
        if err:
            print('--- QEMU STDERR ---')
            print(err.decode('utf-8', errors='replace'))
    except subprocess.TimeoutExpired:
        proc.terminate()
        proc.wait(timeout=3)
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    cleanup_pipes()

    full = ''.join(captured)
    if 'panic' in full.lower() or 'kerneltrap' in full.lower():
        print('\n!!! KERNEL PANIC !!!')
        return 1
    print('\n=== DONE ===')
    return 0


if __name__ == '__main__':
    sys.exit(main())
