import unittest
from PIL import Image
from tools.check_atlas_pixels import check

class AtlasTests(unittest.TestCase):
    def test_black_never_passes(self):
        self.assertFalse(check(Image.new('RGB',(1280,540)))['passed'])
    def test_uniform_green_never_passes_alternation(self):
        self.assertFalse(check(Image.new('RGB',(1280,540),(0,32,0)),clear=True)['passed'])
    def test_reduced_resolution_rejected(self):
        with self.assertRaises(ValueError):check(Image.new('RGB',(20,20)))
