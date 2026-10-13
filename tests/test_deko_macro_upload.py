import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('uploads', Path(__file__).resolve().parents[1]/'tools/deko_macro_upload.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class MacroUploadTests(unittest.TestCase):
    def fixture(self):
        return [0xa0050045, 0, 10, 20, 30, 40, 0xa0030047, 0, 0, 2]

    def test_instruction_memory_and_entries_preserved(self):
        words = module.explicit_uploads(self.fixture())
        self.assertEqual(words, [0xa0050045,0,10,20,30,40,0xa0030045,2,30,40,0xa0030047,0,0,2])

    def test_out_of_bounds_entry_rejected(self):
        data = self.fixture(); data[-1] = 4
        with self.assertRaises(ValueError): module.explicit_uploads(data)

    def test_truncated_table_rejected(self):
        with self.assertRaises(ValueError): module.explicit_uploads(self.fixture()[:-1])

    def test_unexpected_command_rejected(self):
        data = self.fixture(); data[0] = 0xa0050046
        with self.assertRaises(ValueError): module.explicit_uploads(data)

    def test_transform_is_idempotent(self):
        text = 'static constexpr uint32_t data[] = {\n'+''.join('\t0x%08X,\n'%v for v in self.fixture())+'\n};\n'
        transformed = module.transform(text)
        self.assertEqual(transformed, module.transform(transformed))
