#!/usr/bin/env python3
"""Exercise the packaging scripts' failure paths without IDF or a simulator."""

import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HASH = "a" * 64
SOURCE_ARCHIVES = (
    "rubik-e0bf62e069682273f8679f9297decd66079aa9c3.tar.gz",
    "HomebrewOwl.GB-9ac178632e99093b40d418148e82ca1c8392bb2f.tar.gz",
)
NOTICE_FILES = (
    "Peanut-GB-LICENSE", "Peanut-GB-SOURCE.md", "Peanut-GB-NOTICE", "BSP-LICENSE", "BSP-IMPORT.md",
    "dmg-acid2-LICENSE", "mooneye-LICENSE", "galactic-armada-LICENSE",
    "rubik-MIT-LICENSE", "HomebrewOwl.GB-GPL-3.0-LICENSE", "test-roms.md",
)


class PackScriptsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="gb-pack-test-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for directory in ("tools", "bin", "build", "roms/homebrew"):
            (self.root / directory).mkdir(parents=True)
        for name in ("build_device_pack.sh", "test_sim_roms.sh"):
            shutil.copy2(ROOT / "tools" / name, self.root / "tools" / name)
        materials = (
            "LICENSE", "third_party/peanut-gb/LICENSE", "third_party/peanut-gb/SOURCE.md",
            "third_party/peanut-gb/peanut_gb.h",
            "components/bsp/LICENSE", "components/bsp/IMPORT.md", "docs/test-roms.md",
            "roms/third_party/dmg-acid2/LICENSE", "roms/third_party/mooneye/LICENSE",
            "roms/third_party/galactic-armada/LICENSE", "roms/third_party/rubik/LICENSE",
            "roms/third_party/rubik/LICENSE-HomebrewOwl.GPL", "roms/third_party/rubik/SOURCE.md",
        )
        for name in materials:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture material: " + name + "\n")
        fixtures = self.root / "source-fixtures"
        fixtures.mkdir()
        for name in SOURCE_ARCHIVES:
            (fixtures / name).write_bytes(("fixture source: " + name).encode())
        notices_script = ROOT / "tools/package_notices.sh"
        if notices_script.exists():
            # Keep downloads offline: only replace archive checksum pins in the
            # temporary script copy; checksum validation still uses sha256sum.
            script = notices_script.read_text()
            for var, name in zip(("RUBIK_SHA", "OWL_SHA"), SOURCE_ARCHIVES):
                digest = hashlib.sha256((fixtures / name).read_bytes()).hexdigest()
                script = re.sub(r"^" + var + r"=[0-9a-f]{64}$", var + "=" + digest,
                                script, flags=re.MULTILINE)
            self.executable("tools/package_notices.sh", script)
        self.executable("bin/curl", """#!/usr/bin/env python3
import os
from pathlib import Path
import shutil
import sys
if os.environ.get('TEST_SOURCE_DOWNLOAD_FAIL'):
    sys.exit(22)
dest = Path(sys.argv[sys.argv.index('-o') + 1])
url = next(arg for arg in sys.argv if arg.startswith('https://'))
prefix = 'HomebrewOwl.GB-' if '/HomebrewOwl.GB/' in url else 'rubik-'
source = next(Path('source-fixtures').glob(prefix + '*'))
if os.environ.get('TEST_SOURCE_BAD_HASH'):
    dest.write_bytes(b'incorrect source archive')
else:
    shutil.copyfile(source, dest)
""")
        (self.root / "roms/homebrew/pattern.gb").write_bytes(bytes(32768))
        self.executable("tools/fetch_test_roms.sh", "#!/bin/sh\nexit 0\n")
        self.executable("bin/idf.py", """#!/usr/bin/env python3
import os
from pathlib import Path
import sys
if os.environ.get('TEST_IDF_MODE') == 'fail':
    print('injected IDF build failure', file=sys.stderr)
    sys.exit(17)
files = ['merged-binary.bin', 'bootloader/bootloader.bin',
         'partition_table/partition-table.bin', 'ai-passport-gb.bin',
         'ai-passport-gb.elf']
for name in files:
    p = Path('idf-build') / name
    p.parent.mkdir(parents=True, exist_ok=True)
    if name == 'ai-passport-gb.bin' and os.environ.get('TEST_IDF_MODE') == 'missing_app':
        p.unlink(missing_ok=True)
    else:
        p.write_bytes(b'fixture firmware')
""")
        self.executable("build/gb-host", """#!/usr/bin/env python3
import os
import sys
mode = os.environ.get('TEST_HOST_MODE')
if mode != 'empty':
    print('sha256 ' + 'a' * 64)
sys.exit(9 if mode == 'fail_after_hash' else 0)
""")
        self.executable("tools/passportsim", """#!/usr/bin/env python3
import json
import os
from pathlib import Path
import sys
if sys.argv[1] == 'setup':
    sys.exit(0)
out = Path(sys.argv[4])
art = out / 'once/artifacts'
art.mkdir(parents=True, exist_ok=True)
log = out / 'once/session.log'
if out.name == os.environ.get('TEST_SIM_FAIL_NAME'):
    log.write_text('injected simulator failure\\n')
    sys.exit(17)
lines = []
if out.name == 'ram-over':
    lines.append('gb_save: cart RAM 8192 exceeds 2048 byte NVS limit')
elif not os.environ.get('TEST_SIM_NO_HASH'):
    lines.append('gb_hash sha256=' + 'a' * 64)
lines += ['gb_perf frames=60', json.dumps({'receipt': {'timing_lint': []}})]
if os.environ.get('TEST_SIM_LATE_LINT'):
    lines.append(json.dumps({'receipt': {'timing_lint': ['late timing failure']}}))
log.write_text('\\n'.join(lines) + '\\n')
(art / 'panel.png').write_bytes(b'fixture screenshot')
""")

    def executable(self, name, text):
        path = self.root / name
        path.write_text(text)
        path.chmod(0o755)

    def run_script(self, script, **settings):
        env = os.environ.copy()
        env.update(settings)
        env["PATH"] = str(self.root / "bin") + os.pathsep + env["PATH"]
        env["PASSPORTSIM_BIN"] = str(self.root / "tools/passportsim")
        return subprocess.run(
            ["/bin/sh", str(self.root / "tools" / script), str(self.root / "output")],
            cwd=self.root, env=env, text=True, capture_output=True, timeout=30,
        )

    def seed_old_result(self, path):
        path.mkdir(parents=True, exist_ok=True)
        (path / "previous.log").write_text(
            "gb_hash sha256=" + HASH + "\ngb_perf frames=60\n"
            '{"receipt":{"timing_lint":[]}}\n'
        )

    def assert_failed(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sim_failure_cannot_reuse_old_hash(self):
        self.seed_old_result(self.root / "output/acid2/once/artifacts")
        result = self.run_script("test_sim_roms.sh", TEST_SIM_FAIL_NAME="acid2")
        self.assert_failed(result)
        self.assertNotIn("HASH OK acid2", result.stdout)

    def test_pack_failure_cannot_reuse_old_hash(self):
        self.seed_old_result(self.root / "output/sim/pattern/once/artifacts")
        result = self.run_script("build_device_pack.sh", TEST_SIM_FAIL_NAME="pattern")
        self.assert_failed(result)
        self.assertNotIn("OK pattern", result.stdout)

    def test_empty_hashes_cannot_pass_sim_tests(self):
        result = self.run_script("test_sim_roms.sh", TEST_HOST_MODE="empty",
                                 TEST_SIM_NO_HASH="1")
        self.assert_failed(result)

    def test_sim_accepts_current_matching_results(self):
        result = self.run_script("test_sim_roms.sh")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("test_sim_roms: ok", result.stdout)

    def test_build_failure_cannot_package_previous_image(self):
        for name in ("merged-binary.bin", "bootloader/bootloader.bin",
                     "partition_table/partition-table.bin", "ai-passport-gb.bin",
                     "ai-passport-gb.elf"):
            path = self.root / "idf-build" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"previous image")
        result = self.run_script("build_device_pack.sh", TEST_IDF_MODE="fail")
        self.assert_failed(result)
        self.assertNotIn("OK pattern", result.stdout)

    def test_missing_build_output_fails_pack(self):
        result = self.run_script("build_device_pack.sh", TEST_IDF_MODE="missing_app")
        self.assert_failed(result)

    def test_failed_host_hash_check_fails_pack(self):
        result = self.run_script("build_device_pack.sh", TEST_HOST_MODE="fail_after_hash")
        self.assert_failed(result)

    def test_empty_hashes_cannot_pass_pack(self):
        result = self.run_script("build_device_pack.sh", TEST_HOST_MODE="empty",
                                 TEST_SIM_NO_HASH="1")
        self.assert_failed(result)

    def test_late_timing_lint_fails_pack(self):
        result = self.run_script("build_device_pack.sh", TEST_SIM_LATE_LINT="1")
        self.assert_failed(result)

    def test_failed_blargg_check_never_places_images_in_public_pack(self):
        result = self.run_script("build_device_pack.sh", TEST_SIM_FAIL_NAME="blargg-cpu-instrs")
        self.assert_failed(result)
        self.assertEqual(list((self.root / "output").rglob("*blargg*.bin")), [])
        self.assertEqual(list((self.root / "output").rglob("*blargg*.elf")), [])

    def test_missing_notice_fails_pack(self):
        (self.root / "third_party/peanut-gb/LICENSE").unlink()
        self.assert_failed(self.run_script("build_device_pack.sh"))

    def test_source_download_failure_fails_pack(self):
        self.assert_failed(self.run_script("build_device_pack.sh", TEST_SOURCE_DOWNLOAD_FAIL="1"))

    def test_source_checksum_mismatch_fails_pack(self):
        self.assert_failed(self.run_script("build_device_pack.sh", TEST_SOURCE_BAD_HASH="1"))

    def test_successful_pack_has_verifiable_manifest_and_no_blargg_images(self):
        result = self.run_script("build_device_pack.sh")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        output = self.root / "output"
        listed = set()
        for line in (output / "manifest.sha256").read_text().splitlines():
            if line.startswith("#"):
                continue
            digest, name = line.split(maxsplit=1)
            path = output / name
            self.assertNotEqual(path, output / "manifest.sha256")
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest, name)
            listed.add(path.resolve())
        for rom in ("pattern", "dmg-acid2", "galactic-armada", "rubik", "mooneye-daa"):
            for suffix in ("merged.bin", "bootloader.bin", "partition-table.bin", "app.bin"):
                self.assertIn((output / "pack" / rom / (rom + "-" + suffix)).resolve(), listed)
        required = [output / "LICENSE", output / "sources/Rubik-SOURCE.md"]
        required += [output / "notices" / name for name in NOTICE_FILES]
        required += [output / "sources" / name for name in SOURCE_ARCHIVES]
        for path in required:
            self.assertTrue(path.is_file(), str(path))
            self.assertIn(path.resolve(), listed)
        self.assertEqual(list(output.rglob("*blargg*.bin")), [])
        self.assertEqual(list(output.rglob("*blargg*.elf")), [])


if __name__ == "__main__":
    unittest.main()
