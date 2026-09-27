"""Only advertise the new version when its verified archive is available."""
import json
from pathlib import Path
import shutil
import sys
import tarfile

source = Path(sys.argv[1])
with tarfile.open(source) as archive:
    package = json.load(archive.extractfile("manifest.json"))
version = ".".join(map(str, package["version"]))
destination = Path(f"packages/ktm/artifacts/ktm_{version}_kindlehf.kpkg")
destination.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(source, destination)
feed = json.loads(Path("manifest.json").read_text())
feed["packages"]["ktm"]["artifacts"] = [{
    "url": str(destination),
    "version": package["version"],
    "dependencies": package["dependencies"],
    "supported_platforms": package["supported_platforms"],
}]
Path("manifest.json").write_text(json.dumps(feed, indent=2) + "\n")
