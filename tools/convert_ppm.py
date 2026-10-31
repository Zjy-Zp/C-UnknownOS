import glob, os, sys
try:
    from PIL import Image
except ImportError:
    print("NOPIL")
    sys.exit(0)

d = '/mnt/c/cunos/shots'
for f in sorted(glob.glob(os.path.join(d, '*.ppm'))):
    im = Image.open(f)
    out = f[:-4] + '.png'
    im.save(out)
    print(os.path.basename(out), im.size)
