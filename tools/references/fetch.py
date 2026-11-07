#!/usr/bin/env python3
"""Downloads HONKR's reference recordings (references/manifest.tsv) into a cache outside the
repository and checks them against references/checksums.sha256.

    tools/references/fetch.py [--cache DIR] [--pin]

The recordings are the University of Iowa Musical Instrument Samples, free to use without
restriction; they are not committed because of their size (about 180 MB). --pin writes the
checksums file from what was downloaded (do this once, when the manifest changes, and commit it).
Prints the cache directory on success."""
import argparse, hashlib, os, sys, urllib.parse, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SITE = "https://theremin.music.uiowa.edu/"

def manifest():
    rows = []
    for line in open(os.path.join(ROOT, "references", "manifest.tsv")):
        if line.startswith("#") or not line.strip(): continue
        inst, dyn, path = line.rstrip("\n").split("\t")
        rows.append((inst, dyn, path))
    return rows

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""): h.update(chunk)
    return h.hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cache", default=os.path.expanduser("~/.cache/honkr-references"))
    ap.add_argument("--pin", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.cache, exist_ok=True)
    sums_path = os.path.join(ROOT, "references", "checksums.sha256")
    pinned = {}
    if os.path.exists(sums_path):
        for line in open(sums_path):
            digest, name = line.split(None, 1)
            pinned[name.strip()] = digest
    sums, bad = {}, 0
    for inst, dyn, path in manifest():
        name = os.path.basename(path).replace(" ", "")
        dest = os.path.join(a.cache, name)
        if not os.path.exists(dest):
            url = SITE + urllib.parse.quote(path)
            for attempt in range(4):
                try:
                    with urllib.request.urlopen(url, timeout=120) as r, open(dest + ".part", "wb") as f:
                        f.write(r.read())
                    os.replace(dest + ".part", dest)
                    break
                except Exception as e:
                    print(f"retry {attempt + 1} {name}: {e}", file=sys.stderr)
            else:
                print(f"FAILED {name}", file=sys.stderr); bad += 1; continue
        sums[name] = sha256(dest)
        if not a.pin and name in pinned and pinned[name] != sums[name]:
            print(f"CHECKSUM MISMATCH {name}", file=sys.stderr); bad += 1
    if a.pin:
        with open(sums_path, "w") as f:
            for name in sorted(sums): f.write(f"{sums[name]}  {name}\n")
    print(a.cache)
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
