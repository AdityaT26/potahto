#!/usr/bin/env python3
# Wrapper: exuberant-ctags + synthesize Arduino-ctags "returntype:" field.
import subprocess, sys, re
out = subprocess.run(["/usr/bin/ctags-exuberant"] + sys.argv[1:], capture_output=True, text=True)
for line in out.stdout.splitlines():
    if "\tkind:function" in line and "returntype:" not in line:
        name = line.split("\t")[0]
        m = re.search(r'/\^(.*)\$/;"', line)
        rt = None
        if m:
            src = m.group(1)
            i = src.find(name + "(")
            if i < 0: i = src.find(name + " (")
            if i > 0:
                rt = re.sub(r'^(static|inline|extern|virtual|IRAM_ATTR)\s+', '', src[:i].strip()).strip()
        if rt: line += "\treturntype:" + rt
    print(line)
sys.stderr.write(out.stderr); sys.exit(out.returncode)
