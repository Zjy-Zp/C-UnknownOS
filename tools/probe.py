
import json, os, socket, subprocess, time, sys
from PIL import Image

ROOT = r"C:\cunos"
QEMU = r"C:\EXES\qemu\qemu-system-i386.exe"
ISO = os.environ.get("CUNOS_ISO") or os.path.join(ROOT, "cunos_v3.iso")

def run(tag, drive_args, machine=None, cmds=("ls","fs"), wait=9):
    tag = tag + os.environ.get("CUNOS_TAG", "")
    out = os.path.join(ROOT, "shots", tag)
    os.makedirs(out, exist_ok=True)
    port = 4500 + (abs(hash(tag)) % 300)
    args = [QEMU, "-cdrom", ISO, "-m", "256", "-smp", "1", "-boot", "order=d,menu=off"]
    if machine: args += ["-machine", machine]
    args += drive_args + ["-vga", "std", "-display", "none",
                          "-qmp", "tcp:127.0.0.1:%d,server,nowait" % port]
    p = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=ROOT)
    s = None
    for _ in range(80):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=1); break
        except OSError: time.sleep(0.2)
    if s is None:
        print(tag, "QMP FAIL"); p.kill(); return None
    f = s.makefile("rwb"); f.readline()
    f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()
    def q(cmd, **a):
        f.write(json.dumps({"execute":cmd,"arguments":a}).encode()+b"\n"); f.flush()
        try: f.readline()
        except Exception: pass
    def keys(w):
        for c in w:
            q("send-key", keys=[{"type":"qcode","data":c}]); time.sleep(0.05)
    time.sleep(wait)
    shots = []
    q("screendump", filename=os.path.join(out, "00_boot.ppm")); time.sleep(0.6)
    for i, c in enumerate(cmds, 1):
        keys(c); q("send-key", keys=[{"type":"qcode","data":"ret"}]); time.sleep(2.2)
        fn = os.path.join(out, "%02d_%s.ppm" % (i, c))
        q("screendump", filename=fn); time.sleep(0.5); shots.append(fn)
    p.kill(); p.wait(); s.close()
    for pp in shots:
        Image.open(pp).save(pp[:-4] + ".png")
    print(tag, "ok ->", out)
    return out

if __name__ == "__main__":
    mode = sys.argv[1]
    D = os.environ.get("CUNOS_DISK") or os.path.join(ROOT, "disks", "disk.img")
    print("disk:", D)
    if mode == "q35":
        run("q35", ["-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % D], machine="q35")
    elif mode == "sata":
        run("sata", ["-device", "ich9-ahci,id=ah", "-drive", "file=%s,format=raw,if=none,id=d0" % D,
                     "-device", "ide-hd,drive=d0,bus=ah.0"], machine="q35")
    elif mode == "ide":
        run("ide", ["-drive", "file=%s,format=raw,if=ide,index=0,media=disk" % D])
    elif mode == "virtio":
        run("virtio", ["-drive", "file=%s,format=raw,if=virtio" % D])
