import unittest
from PIL import Image
from tools.check_xenos_shader_pixels import check
class ShaderCaptureTests(unittest.TestCase):
    def test_black_and_flat_colors_fail(self):
        for color in ((0,0,0),(128,32,48),(64,64,48)):
            self.assertFalse(check(Image.new('RGB',(1280,720),color))['passed'])
    def test_too_small_rejected(self):
        with self.assertRaises(ValueError):check(Image.new('RGB',(16,16)))
