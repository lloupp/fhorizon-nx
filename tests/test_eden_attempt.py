from pathlib import Path
import tempfile
import json
import unittest
from PIL import Image
from tools.eden_attempt import summarize

class AttemptTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.work=Path(self.temp.name)
        (self.work/'captures').mkdir();Image.new('RGB',(64,64),(64,128,191)).save(self.work/'captures/boot.png')
        self.trace=self.work/'data/eden/sdmc';self.trace.mkdir(parents=True)
        self.report=dict(nro_sha256='synthetic',input_exit_verified=True,emulator_process_exit_code=0,emulator_process_timeout=False)
        self.events='boot\nconstants_96_frames_verified\ntelemetry process_used=1234\nshutdown'
    def tearDown(self):self.temp.cleanup()
    def result(self):
        (self.work/'report.json').write_text(json.dumps(self.report))
        (self.trace/'trace.log').write_text('\n'.join('FHNX_GRAPHICS '+e for e in self.events.splitlines()))
        (self.work/'eden.log').write_text('Driver: synthetic-driver\n')
        return summarize(self.work,script_exit=0)
    def test_complete(self):
        r=self.result();self.assertTrue(r['approved']);self.assertEqual(r['process_memory'],1234);self.assertEqual(r['driver'],'synthetic-driver')
    def test_guest_failure_even_after_pass_label(self):
        self.events+='\nconstants_probe_failed';self.assertFalse(self.result()['approved'])
    def test_wrong_pixels(self):
        self.events+='\nconstants_frame pixels=FAIL';self.assertFalse(self.result()['approved'])
    def test_signal(self):
        self.report['emulator_process_exit_code']=-11;r=self.result();self.assertFalse(r['approved']);self.assertEqual(r['signal'],'SIGSEGV')
    def test_timeout(self):
        self.report['emulator_process_timeout']=True;self.assertFalse(self.result()['approved'])
    def test_black_capture(self):
        Image.new('RGB',(64,64)).save(self.work/'captures/boot.png');self.assertFalse(self.result()['approved'])
    def test_partial_has_no_pixel_approval(self):
        self.events='boot\nresources_ready\nshutdown';self.assertFalse(self.result()['approved'])
    def test_zero_exit_is_not_guest_exit(self):
        self.report['input_exit_verified']=False;self.assertFalse(self.result()['approved'])
