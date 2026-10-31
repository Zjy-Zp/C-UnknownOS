"""Write ACPI PM1_CNT from the QEMU monitor to verify the port/model."""
import json, os, socket, subprocess, time

ROOT = r"C:\cunos"
QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = os.path.join(ROOT, "cunos.iso")
DISK = os.path.join(ROOT, "disk.img")
PORT = 4458

p = subprocess.Popen([
    QEMU, "-cdrom", ISO, "-m", "256", "-smp", "1",
    "-boot", "order=d,menu=off",
    "-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % DISK,
    "-vga", "std", "-display", "none",
    "-qmp", "tcp:127.0.0.1:%d,server,nowait" % PORT,
], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

s = None
for _ in range(50):
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=1); break
    except OSError:
        time.sleep(0.2)
f = s.makefile("rwb")
f.readline()
f.write(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n"); f.flush(); f.readline()

def q(cmd, **args):
    f.write(json.dumps({"execute": cmd, "arguments": args}).encode() + b"\n"); f.flush()
    return f.readline().decode(errors="replace").strip()

time.sleep(9)
print("info:", q("human-monitor-command", **{"command-line": "info mtree -f"})[:0])
print("write outl 0x604 0x3400:", q("human-monitor-command", **{"command-line": "outl 0x604 0x3400"}))
time.sleep(3)
print("status:", q("query-status"))
print("exited:", p.poll() is not None)
if p.poll() is None:
    p.kill(); p.wait()
