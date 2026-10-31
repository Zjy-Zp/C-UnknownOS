#!/usr/bin/env python3
"""cunos 自动测试：启动 QEMU + QMP 截图 + 键盘输入 + ACPI 关机验证"""
import json
import os
import socket
import subprocess
import sys
import time

QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = r"C:\cunos\cunos.iso"
DISK = r"C:\cunos\disks\disk.img"
SHOTS = r"C:\cunos\shots\run2"
PORT = 4471

os.makedirs(SHOTS, exist_ok=True)


class QMP:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=15)
        self.buf = b""
        self._read_obj()
        self.cmd("qmp_capabilities")

    def _recv(self):
        while b"\n" not in self.buf:
            data = self.s.recv(65536)
            if not data:
                raise RuntimeError("QMP closed")
            self.buf += data
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line.decode())

    def _read_obj(self):
        while True:
            obj = self._recv()
            if "event" not in obj:
                return obj

    def cmd(self, name, args=None):
        msg = {"execute": name}
        if args:
            msg["arguments"] = args
        self.s.sendall((json.dumps(msg) + "\n").encode())
        return self._read_obj()

    def hmp(self, line):
        return self.cmd("human-monitor-command", {"command-line": line})

    def shot(self, name):
        path = os.path.join(SHOTS, name + ".ppm")
        if os.path.exists(path):
            os.remove(path)
        self.hmp("screendump %s" % path)
        for _ in range(20):
            if os.path.exists(path) and os.path.getsize(path) > 1000:
                break
            time.sleep(0.2)
        return path

    def key(self, keys, delay=0.06):
        for k in keys:
            self.hmp("sendkey " + k)
            time.sleep(delay)

    MAP = {" ": "spc", "\n": "ret", "-": "minus", ".": "dot", "/": "slash",
           "_": "shift-minus", ",": "comma", ":": "shift-semicolon"}

    def type(self, text):
        for ch in text:
            if ch in self.MAP:
                self.key([self.MAP[ch]])
            elif ch.isupper():
                self.key(["shift-" + ch.lower()])
            else:
                self.key([ch])


def png(ppm, out):
    try:
        from PIL import Image
        Image.open(ppm).save(out)
        return True
    except Exception as e:
        print("   ! png fail", e)
        return False


def main():
    proc = subprocess.Popen([
        QEMU, "-cdrom", ISO, "-m", "256", "-smp", "1",
        "-boot", "order=d,menu=off",
        "-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % DISK,
        "-usb", "-device", "usb-tablet", "-device", "usb-ehci",
        "-vga", "std", "-display", "none",
        "-qmp", "tcp:127.0.0.1:%d,server,nowait" % PORT,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    time.sleep(2.0)
    q = QMP(PORT)
    print("QMP connected, waiting for boot ...")

    steps = []

    def snap(tag):
        p = q.shot(tag)
        out = os.path.join(SHOTS, tag + ".png")
        ok = png(p, out)
        steps.append((tag, ok, os.path.getsize(out) if ok and os.path.exists(out) else 0))
        print("  shot %-16s -> %s (%s)" % (tag, tag + ".png", "ok" if ok else "fail"))

    time.sleep(9)
    snap("01_boot")

    q.type("ver\n"); time.sleep(1.2); snap("02_ver")
    q.type("ls\n"); time.sleep(1.0); snap("03_ls")
    q.type("pci\n"); time.sleep(1.0); snap("04_pci")
    q.type("usb\n"); time.sleep(1.0); snap("05_usb")
    q.type("echo hello cunos\n"); time.sleep(0.8); snap("06_echo")

    q.type("enter t.txt\n"); time.sleep(0.8)
    q.type("hello from cunos fat32\n"); time.sleep(0.4)
    q.type("second line ok\n"); time.sleep(0.4)
    q.type(".\n"); time.sleep(1.0); snap("07_enter")
    q.type("ls\n"); time.sleep(1.0); snap("08_ls_after")
    q.type("del t.txt\n"); time.sleep(1.0); snap("09_del")
    q.type("ls\n"); time.sleep(1.0); snap("10_ls_final")

    q.type("desktop\n"); time.sleep(1.5); snap("11_desktop")
    q.type("\n"); time.sleep(1.0); snap("12_back")

    q.type("edit note.txt\n"); time.sleep(1.5); snap("13_editor")
    q.type("nano works\n"); time.sleep(0.5)
    q.key(["ctrl-o"]); time.sleep(0.8)
    q.key(["ctrl-x"]); time.sleep(0.8); snap("14_after_editor")
    q.type("ls\n"); time.sleep(1.0); snap("15_ls_note")

    print("--- shutdown test ---")
    q.type("shutdown\n")
    exited = False
    for _ in range(30):
        if proc.poll() is not None:
            exited = True
            break
        time.sleep(0.5)

    print("ACPI shutdown -> QEMU exited:", exited, "rc=", proc.poll())
    if not exited:
        proc.kill()

    print("\nsummary:")
    for tag, ok, size in steps:
        print("  %-16s %s %d" % (tag, "OK" if ok else "FAIL", size))
    print("shutdown:", "PASS" if exited else "FAIL")


if __name__ == "__main__":
    main()
