from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from fhnx import audit_public_tree, doctor, inspect_game

class PreflightTests(unittest.TestCase):
    def test_doctor(self):
        d = doctor()
        self.assertIn('tools', d)
        self.assertIn('switch_toolchain_ready', d)

    def test_synthetic_executable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ('default.xex', 'XMediaFacade_default.xex', 'SpeechFacade_default.xex'):
                (root / name).write_bytes(b'XEX2' + b'\x00' * 60)
            out = inspect_game(root)
            self.assertTrue(out['all_expected_modules_present'])
            self.assertFalse(out['verified_revision'])
            self.assertEqual(len(out['modules'][0]['sha256']), 64)

    def test_fake_magic_not_accepted(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / 'default.xex').write_bytes(b'FAKE')
            self.assertFalse(inspect_game(Path(tmp))['all_expected_modules_present'])

    def test_iso_is_metadata_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            iso = Path(tmp) / 'game.iso'
            iso.write_bytes(b'synthetic')
            result = inspect_game(iso)
            self.assertEqual(result['input_type'], 'file')
            self.assertNotIn('modules', result)

    def test_audit_rejects_proprietary_files_and_generated(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'main.cpp').write_text('int main(){}')
            self.assertTrue(audit_public_tree(root)['safe_for_review'])
            (root / 'default.xex').write_bytes(b'XEX2')
            (root / 'generated').mkdir()
            self.assertFalse(audit_public_tree(root)['safe_for_review'])

if __name__ == '__main__':
    unittest.main()
