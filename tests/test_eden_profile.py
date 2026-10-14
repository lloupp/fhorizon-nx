from pathlib import Path
import tempfile
import unittest
from tools.eden_profile import write_gpu_accuracy

class ProfileTests(unittest.TestCase):
    def test_high_is_explicit_without_absolute_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'config.ini'
            write_gpu_accuracy(path,'high')
            self.assertEqual(path.read_text(),'[Renderer]\ngpu_accuracy\\default=false\ngpu_accuracy=2\n')
            with self.assertRaises(ValueError):write_gpu_accuracy(path,'medium')
    def test_unknown_mode_rejected_without_writing(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'config.ini'
            with self.assertRaises(ValueError):write_gpu_accuracy(path,'invented')
            self.assertFalse(path.exists())
