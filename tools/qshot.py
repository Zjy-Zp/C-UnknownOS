#!/usr/bin/env python3
"""快速单次启动 + 截图：python tools/qshot.py [等待秒] [前缀] [按键序列用|分隔]"""
import json, os, socket, subprocess, sys, time

QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = r"C:\cunos\cunos.iso"
DISK = r"C:\cunos\disks\disk.img"
SHOTS = r"C:\cunos\shots\dbg"
PORT = 4483
os.makedirs(SHOTS, exist_ok=True)


class QMP:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=15)
        self.buf = b""; self._read_obj(); self.cmd("qmp_capabilities")

    def _recv(self):
        while b"\n" not in self.buf:
            d = self.s.recv(65536)
            if not d: raise RuntimeError("closed")
            self.buf += d
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line.decode())

    def _read_obj(self):
        while True:
            o = self._recv()
            if "event" not in o: return o

    def cmd(self, name, args=None):
        m = {"execute": name}
        if args: m["arguments"] = args
        self.s.sendall((json.dumps(m) + "\n").encode())
        return self._read_obj()

    def hmp(self, line): return self.cmd("human-monitor-command", {"command-line": line})

    def shot(self, name):
        p = os.path.join(SHOTS, name + ".ppm")
        if os.path.exists(p): os.remove(p)
        self.hmp("screendump %s" % p)
        for _ in range(20):
            if os.path.exists(p) and os.path.getsize(p) > 1000: break
            time.sleep(0.2)
        from PIL import Image
        im = Image.open(p); im.save(os.path.join(SHOTS, name + ".png"))
        return im.size


def main():
    wait = float(sys.argv[1]) if len(sys.argv) > 1 else 10
    prefix = sys.argv[2] if len(sys.argv) > 2 else "s"
    keys = sys.argv[3].split("|") if len(sys.argv) > 3 else []

    proc = subprocess.Popen([QEMU, "-cdrom", ISO, "-m", "256",
                             "-boot", "order=d,menu=off",
                             "-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % DISK,
                             "-vga", "std", "-display", "none",
                             "-qmp", "tcp:127.0.0.1:%d,server,nowait" % PORT],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.5)
    q = QMP(PORT)
    time.sleep(wait)
    print(prefix, q.shot(prefix))
    for i, k in enumerate(keys):
        q.hmp("sendkey " + k); time.sleep(0.35)
        if k == "ret" or k.startswith("ctrl"):
            time.sleep(0.6)
            print("%s_%02d" % (prefix, i), q.shot("%s_%02d" % (prefix, i)))
    proc.kill()


if __name__ == "__main__":
    main()
