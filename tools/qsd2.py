"""Shutdown-only probe: boot cunos, type `shutdown`, check if QEMU exits."""
import json, os, socket, subprocess, sys, time

ROOT = r"C:\cunos"
QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = os.path.join(ROOT, "cunos.iso")
DISK = os.path.join(ROOT, "disk.img")
OUT = os.path.join(ROOT, "shots", "s5")
PORT = 4457

def main():
    os.makedirs(OUT, exist_ok=True)
    if not os.path.exists(DISK):
        with open(DISK, "wb") as f:
            f.truncate(64 * 1024 * 1024)
    p = subprocess.Popen([
        QEMU, "-cdrom", ISO, "-m", "256", "-smp", "1",
        "-boot", "order=d,menu=off",
        "-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % DISK,
        "-usb", "-device", "usb-tablet", "-device", "usb-ehci",
        "-vga", "std", "-display", "none", "-machine", "pc,smm=off",
        "-qmp", "tcp:127.0.0.1:%d,server,nowait" % PORT,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    s = None
    for _ in range(50):
        try:
            s = socket.create_connection(("127.0.0.1", PORT), timeout=1)
            break
        except OSError:
            time.sleep(0.2)
    if s is None:
        print("QMP connect failed"); p.kill(); return
    f = s.makefile("rwb")
    f.readline()
    f.write(json.dumps({"execute": "qmp_capabilities"}).encode() + b"\n"); f.flush(); f.readline()

    def q(cmd, **args):
        f.write(json.dumps({"execute": cmd, "arguments": args}).encode() + b"\n"); f.flush()
        f.readline()

    time.sleep(9)
    for k in ["s", "h", "u", "t", "d", "o", "w", "n", "ret"]:
        q("send-key", keys=[{"type": "qcode", "data": k}])
        time.sleep(0.08)

    time.sleep(2)
    q("screendump", filename=os.path.join(OUT, "before_exit.ppm"))
    time.sleep(4)

    try:
        f.write(json.dumps({"execute": "query-status"}).encode() + b"\n"); f.flush()
        print("status:", f.readline().decode(errors="replace").strip())
    except Exception as e:
        print("status query failed:", e)

    rc = p.poll()
    print("QEMU exited:", rc is not None, "rc=", rc)
    if rc is None:
        try:
            q("screendump", filename=os.path.join(OUT, "still_alive.ppm"))
        except Exception as e:
            print("screendump fail", e)
        p.kill()
        p.wait()
    s.close()

if __name__ == "__main__":
    main()

