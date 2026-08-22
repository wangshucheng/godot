import re
data = open(r'..\bin\godot.web.template_release.wasm32.nothreads.mono.js', 'rb').read().decode('utf-8', errors='replace')
for pat in ['preRun', 'onRuntimeInitialized', 'Module["FS"]', 'this.FS', 'engine.FS']:
    idx = data.find(pat)
    print('===', pat, '->', 'FOUND' if idx >= 0 else 'not found')
    if idx >= 0:
        s = max(0, idx - 150); e = min(len(data), idx + 200)
        print(data[s:e].replace('\n', ' ')[:350])
