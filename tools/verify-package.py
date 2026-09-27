"""Reject runtime dependencies and incomplete launcher files before publishing."""
import json
import struct
import sys
import tarfile

with tarfile.open(sys.argv[1], "r:gz") as archive:
    names = archive.getnames()
    manifest = json.load(archive.extractfile("manifest.json"))
    assert manifest["manifest_version"] == 2
    assert manifest["supported_platforms"] == ["kindlehf"]
    assert manifest["dependencies"] == [{"id": "kterm", "min": [2, 6, 0]}]
    assert "bin/ktmterm" not in names, "Unverified GTK terminal must not return"
    for path in ("launch.sh", "session.sh", "ui.sh", "install.sh", "uninstall.sh",
                 "scriptlet/ktm.sh", "bin/ktm", "bin/ktm-input"):
        member = archive.getmember(path)
        assert member.mode & 0o100, f"{path} must be executable"
    for path in ("bin/ktm", "bin/ktm-input"):
        data = archive.extractfile(path).read()
        assert data[:6] == b"\x7fELF\x01\x01", "Expected 32-bit little-endian ELF"
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        assert header[2] == 40, "Expected ARM"
        assert header[7] & 0x400, "Expected hard-float ABI"
        for i in range(header[10]):
            ph = struct.unpack_from("<IIIIIIII", data, header[5] + i * header[9])
            assert ph[0] not in (2, 3), "No dynamic linker or shared-library dependencies allowed"
    launch = archive.extractfile("launch.sh").read()
    assert b'launch kterm -e' in launch
    assert b'ktmterm' not in launch
print("Verified manifest v2, executable lifecycle files, official KTerm launcher, static ARM helpers")
