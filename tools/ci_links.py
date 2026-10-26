#!/usr/bin/env python3
"""Linux (sistema de arquivos que diferencia caixa): cria em port/compat_ci links para os headers incluidos com outra caixa.
Uso: python3 tools/ci_links.py   (rode depois do setup_engine.sh; idempotente)"""
import os, re, sys
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
dirs = ['port/compat', 'port/compat_ci', 'port/n64', 'port/gta2_re/Source', 'port/gta2_re']
dirs = [os.path.join(root, d) for d in dirs]
idx = {}
for d in dirs:
    if not os.path.isdir(d): continue
    for dp, dn, fn in os.walk(d):
        for f in fn:
            rel = os.path.relpath(os.path.join(dp, f), d).replace('\\', '/')
            idx.setdefault(rel.lower(), os.path.join(dp, f))
inc = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')
need = set()
for sub in ['port', ]:
    for dp, dn, fn in os.walk(os.path.join(root, sub)):
        if '3rdParty' in dp: continue
        for f in fn:
            if f.endswith(('.cpp', '.hpp', '.h', '.c', '.inl')):
                try: txt = open(os.path.join(dp, f), errors='ignore').read()
                except: continue
                for m in inc.finditer(txt): need.add(m.group(1).replace('\\', '/'))
made = 0
for n in sorted(need):
    exact = any(os.path.exists(os.path.join(d, n)) for d in dirs)
    if exact: continue
    t = idx.get(n.lower())
    if not t: continue
    dst = os.path.join(root, 'port/compat_ci', n)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.lexists(dst):
        os.symlink(t, dst); made += 1
print('links criados:', made)
