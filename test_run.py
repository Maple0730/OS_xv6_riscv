#!/usr/bin/env python3
import subprocess
import time
import os
import select
import sys

subprocess.run(["pkill", "-f", "qemu-system"], capture_output=True)
time.sleep(1)

print("Starting QEMU...")
proc = subprocess.Popen(
    ["make", "qemu"],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    stdin=subprocess.PIPE,
    cwd="/home/tfc/OS/OS_xv6_riscv",
    text=True,
    bufsize=1
)

output = []

def read_output(timeout=0.3):
    try:
        while True:
            ready, _, _ = select.select([proc.stdout], [], [], timeout)
            if not ready:
                break
            char = os.read(proc.stdout.fileno(), 4096)
            if char:
                c = char.decode('utf-8', errors='replace')
                output.append(c)
    except:
        pass

print("Waiting for boot...")
boot_done = False
for i in range(60):
    read_output(0.3)
    full_output = ''.join(output)
    if "init: starting" in full_output:
        boot_done = True
        print("Boot complete!")
        break

if not boot_done:
    print("Boot failed")
    proc.terminate()
    sys.exit(1)

# Test semtest2
print("\n=== Testing semtest2 ===")
try:
    proc.stdin.write("semtest2\n")
    proc.stdin.flush()
except:
    pass

time.sleep(20)
read_output(0.5)
full_output = ''.join(output)
lines = full_output.split('\n')
for line in lines[-80:]:
    print(line)

# Check result
if "PASSED" in full_output:
    print("\n=== semtest2 PASSED ===")
elif "FAILED" in full_output:
    print("\n=== semtest2 FAILED ===")
elif "scause" in full_output:
    print("\n=== semtest2 CRASHED ===")
else:
    print("\n=== semtest2: UNKNOWN RESULT ===")

# Send halt
try:
    proc.stdin.write("halt\n")
    proc.stdin.flush()
except:
    pass

time.sleep(1)
proc.terminate()
proc.wait(timeout=5)
