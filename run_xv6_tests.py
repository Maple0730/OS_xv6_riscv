import subprocess
import time
import os
import sys
import fcntl

os.chdir('/home/tfc/OS/OS_xv6_riscv')

# Clean up
pkill_result = subprocess.run(['pkill', '-9', '-f', 'qemu'], capture_output=True)
time.sleep(1)

# Remove old pipes
for p in ['/tmp/xv6_pipe.in', '/tmp/xv6_pipe.out']:
    try:
        os.unlink(p)
    except OSError:
        pass

# Create named pipes
os.mkfifo('/tmp/xv6_pipe.in')
os.mkfifo('/tmp/xv6_pipe.out')
print("Pipes created", flush=True)

# Start QEMU
proc = subprocess.Popen(
    [
        'qemu-system-riscv64',
        '-machine', 'virt',
        '-bios', 'none',
        '-kernel', 'build/kernel/kernel',
        '-m', '128M',
        '-smp', '1',
        '-nographic',
        '-display', 'none',
        '-global', 'virtio-mmio.force-legacy=false',
        '-drive', 'file=build/fs.img,if=none,format=raw,id=x0',
        '-device', 'virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0',
        '-serial', 'pipe:/tmp/xv6_pipe',
    ],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)

print(f"Started QEMU with PID {proc.pid}", flush=True)

output = ""

def read_all(fd, duration):
    """Read from fd for duration seconds"""
    global output
    end_time = time.time() + duration
    while time.time() < end_time:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if ready:
            try:
                data = os.read(fd, 4096)
                if data:
                    text = data.decode('utf-8', errors='replace')
                    output += text
                    print(text, end='', flush=True)
            except BlockingIOError:
                pass
            except OSError as e:
                print(f"OSError: {e}", flush=True)
                break

import select

# Open the output pipe for reading
# Need to open it in a way that doesn't block
out_fd = os.open('/tmp/xv6_pipe.out', os.O_RDONLY | os.O_NONBLOCK)
print(f"Opened output pipe, fd={out_fd}", flush=True)

# Wait for boot
print("Waiting for boot...", flush=True)
read_all(out_fd, 12)

# Send shmtest
print("\n=== Sending shmtest ===", flush=True)
in_fd = os.open('/tmp/xv6_pipe.in', os.O_WRONLY | os.O_NONBLOCK)
os.write(in_fd, b"shmtest\n")
read_all(out_fd, 6)

# Send schedstat
print("\n=== Sending schedstat ===", flush=True)
os.write(in_fd, b"schedstat\n")
read_all(out_fd, 4)

# Send mlfqtest
print("\n=== Sending mlfqtest ===", flush=True)
os.write(in_fd, b"mlfqtest\n")
read_all(out_fd, 4)

# Send schedlatency
print("\n=== Sending schedlatency ===", flush=True)
os.write(in_fd, b"schedlatency\n")
read_all(out_fd, 4)

# Halt
print("\n=== Sending halt ===", flush=True)
os.write(in_fd, b"halt\n")
read_all(out_fd, 2)

# Cleanup
os.close(in_fd)
os.close(out_fd)
proc.terminate()
try:
    proc.wait(timeout=5)
except subprocess.TimeoutExpired:
    proc.kill()

# Remove pipes
for p in ['/tmp/xv6_pipe.in', '/tmp/xv6_pipe.out']:
    try:
        os.unlink(p)
    except:
        pass

# Check results
if 'panic' in output.lower() or 'kerneltrap' in output.lower():
    print("\n\n!!! FAILURE DETECTED - PANIC OR KERNEL TRAP !!!")
    print("Last 500 chars of output:")
    print(output[-500:])
    sys.exit(1)
else:
    print("\n\n=== ALL TESTS COMPLETED ===")
    print(f"Total output: {len(output)} chars")
