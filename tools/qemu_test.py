#!/usr/bin/env python3
"""启动 QEMU 并通过 QMP 交互：截图 + 显存 dump + 发送按键"""
import json
import os
import socket
import subprocess
import sys
import time

QMP_PORT = 4456
SHOT_DIR = '/mnt/c/cunos/shots'
ISO = '/mnt/c/cunos/cunos.iso'
DISK = '/mnt/c/cunos/disks/disk.img'


class QMP:
    def __init__(self, port):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=10)
        self.buf = b''
        self._read_obj()
        self.cmd('qmp_capabilities')

    def _recv(self):
        while b'\n' not in self.buf:
            data = self.s.recv(65536)
            if not data:
                raise RuntimeError('QMP closed')
            self.buf += data
        line, self.buf = self.buf.split(b'\n', 1)
        return json.loads(line.decode())

    def _read_obj(self):
        while True:
            obj = self._recv()
            if 'event' not in obj:
                return obj

    def cmd(self, name, args=None):
        msg = {'execute': name}
        if args:
            msg['arguments'] = args
        self.s.sendall((json.dumps(msg) + '\n').encode())
        return self._read_obj()

    def hmp(self, line):
        return self.cmd('human-monitor-command', {'command-line': line})

    def shot(self, name):
        path = os.path.join(SHOT_DIR, name)
        self.cmd('screendump', {'filename': path})
        time.sleep(0.4)
        return os.path.exists(path)

    def vga(self, name):
        """dump 文本显存 0xB8000 4000 字节"""
        path = os.path.join(SHOT_DIR, name)
        self.hmp('pmemsave 0xb8000 4000 %s' % path)
        time.sleep(0.3)
        return os.path.exists(path)

    def capture(self, tag):
        ok1 = self.shot(tag + '.ppm')
        ok2 = self.vga(tag + '.bin')
        print('  cap %-18s shot=%s vga=%s' % (tag, ok1, ok2))

    def key(self, keys, delay=0.1):
        for k in keys:
            self.hmp('sendkey ' + k)
            time.sleep(delay)

    def type(self, text):
        mapping = {' ': 'spc', '\n': 'ret', '-': 'minus', '.': 'dot',
                   '/': 'slash', '_': 'shift-minus', ',': 'comma',
                   ':': 'shift-semicolon'}
        for ch in text:
            if ch in mapping:
                self.key([mapping[ch]])
            elif ch.isupper():
                self.key(['shift-' + ch.lower()])
            else:
                self.key([ch])


def main():
    os.makedirs(SHOT_DIR, exist_ok=True)
    for f in os.listdir(SHOT_DIR):
        p = os.path.join(SHOT_DIR, f)
        if os.path.isfile(p):
            os.remove(p)

    log = open('/tmp/qemu.log', 'wb')
    proc = subprocess.Popen([
        'qemu-system-i386',
        '-cdrom', ISO,
        '-drive', 'file=%s,format=raw,if=ide,index=0' % DISK,
        '-boot', 'order=d',
        '-m', '128',
        '-display', 'none',
        '-qmp', 'tcp:127.0.0.1:%d,server,nowait' % QMP_PORT,
        '-no-reboot',
        '-serial', 'none',
    ], stdout=log, stderr=subprocess.STDOUT)

    deadline = time.time() + 15
    q = None
    while time.time() < deadline:
        try:
            q = QMP(QMP_PORT)
            break
        except Exception:
            time.sleep(0.5)
    if not q:
        print('QMP 连接失败')
        proc.kill()
        return 1

    print('QEMU 已启动，等待引导...')
    time.sleep(9)
    q.capture('01_boot')

    q.type('ls\n'); time.sleep(1.0); q.capture('02_ls')
    q.type('ver\n'); time.sleep(0.8); q.capture('03_ver')
    q.type('echo hello cunos fat32 acpi ps2 amd nvidia\n'); time.sleep(0.8); q.capture('04_echo')

    q.type('enter test.txt\n'); time.sleep(0.6)
    q.type('hello from cunos enter\n')
    q.type('second line\n')
    q.type('.\n'); time.sleep(1.2); q.capture('05_enter')

    q.type('ls\n'); time.sleep(1.0); q.capture('06_ls_after')
    q.type('del test.txt\n'); time.sleep(1.0); q.capture('07_del')
    q.type('ls\n'); time.sleep(0.8); q.capture('08_ls_final')
    q.type('fs\n'); time.sleep(0.8); q.capture('09_fs')

    q.type('edit note.txt\n'); time.sleep(0.9); q.capture('10_edit')
    q.type('cunos nano editor test\n'); time.sleep(0.6); q.capture('11_edit_typed')
    q.key(['ctrl-x']); time.sleep(0.9); q.capture('12_after_exit')

    q.type('ls\n'); time.sleep(0.8); q.capture('13_ls_note')
    q.type('cat note.txt\n'); time.sleep(0.9); q.capture('14_cat')

    print('qemu alive before shutdown:', proc.poll() is None)
    q.type('shutdown\n')
    for _ in range(20):
        if proc.poll() is not None:
            break
        time.sleep(0.5)
    print('qemu exit code:', proc.poll())
    if proc.poll() is None:
        proc.kill()
        print('关机未生效（进程需强制结束）')
    return 0


if __name__ == '__main__':
    sys.exit(main())
