from pathlib import Path
import zipfile
import shutil

root = Path(__file__).resolve().parents[1]
build = root / "build" / "out" / "opsucht_inventarwert"
if not build.exists():
    raise SystemExit("Build-Ausgabe nicht gefunden.")

out = root / "Opsucht-Inventarwert-Schritt-1.zip"
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for p in build.rglob("*"):
        if p.is_file():
            z.write(p, p.relative_to(build))
print(out)
