
import json, os, socket, subprocess, time, zlib, struct

ROOT = r"C:\cunos"
QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = os.path.join(ROOT, "cunos_v2.iso")
DISK = os.path.join(ROOT, "disks", "disk.img")
OUT = os.path.join(ROOT, "shots", "fatprobe")
PORT = 4488

def ppm2png(src, dst):
    data = open(src, "rb").read()
    i, fields = 0, []
    while len(fields) < 4 and i < len(data):
        nl = data.find(b"\n", i); tok = data[i:nl].strip()
        if tok and not tok.startswith(b"#"): fields.append(tok)
        i = nl + 1
    w, h = map(int, fields[1].split())
    raw, stride = data[i:], w * 3
    rows = b"".join(b"\x00" + raw[y*stride:(y+1)*stride] for y in range(h))
    def ch(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t+d) & 0xffffffff)
    open(dst, "wb").write(b"\x89PNG\r\n\x1a\n"
        + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
        + ch(b"IDAT", zlib.compress(rows, 6)) + ch(b"IEND", b""))

os.makedirs(OUT, exist_ok=True)
p = subprocess.Popen([
    QEMU, "-cdrom", ISO, "-m", "256", "-smp", "1",
    "-boot", "order=d,menu=off",
    "-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % DISK,
    "-vga", "std", "-display", "none",
    "-qmp", "tcp:127.0.0.1:%d,server,nowait" % PORT,
], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

s = None
for _ in range(60):
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=1); break
    except OSError: time.sleep(0.2)
f = s.makefile("rwb"); f.readline()
f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()

def q(cmd, **args):
    f.write(json.dumps({"execute": cmd, "arguments": args}).encode()+b"\n"); f.flush()
    f.readline()

def keys(word):
    for c in word:
        q("send-key", keys=[{"type":"qcode","data":c}])
        time.sleep(0.06)

time.sleep(9)
q("screendump", filename=os.path.join(OUT, "01_boot.ppm")); time.sleep(0.8)
keys("ls"); q("send-key", keys=[{"type":"qcode","data":"ret"}]); time.sleep(2.5)
q("screendump", filename=os.path.join(OUT, "02_ls.ppm")); time.sleep(0.8)
keys("fs"); q("send-key", keys=[{"type":"qcode","data":"ret"}]); time.sleep(2)
q("screendump", filename=os.path.join(OUT, "03_fs.ppm")); time.sleep(0.8)
keys("ver"); q("send-key", keys=[{"type":"qcode","data":"ret"}]); time.sleep(1.5)
q("screendump", filename=os.path.join(OUT, "04_ver.ppm")); time.sleep(0.8)

try:
    f.write(b'{"execute":"query-status"}\n'); f.flush()
    print("status:", f.readline().decode(errors="replace").strip())
except Exception as e:
    print("status err", e)
p.kill(); p.wait()
s.close()

for n in ["01_boot","02_ls","03_fs","04_ver"]:
    ppm2png(os.path.join(OUT, n+".ppm"), os.path.join(OUT, n+".png"))
print("shots ->", OUT)
