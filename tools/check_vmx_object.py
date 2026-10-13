#!/usr/bin/env python3
"""Gate native instructions in named runtime-input VMX test entry points."""
import json
import re
import sys
from pathlib import Path

def check(text):
    functions={}
    current=None
    for line in text.splitlines():
        header=re.match(r'^[0-9a-f]+ <([^>]+)>:',line)
        if header:
            current=header[1];functions[current]=[]
        elif current:
            functions[current].append(line)
    bodies={name:'\n'.join(lines) for name,lines in functions.items()}
    expected={
        'vmx_permute':r'\btbl\b',
        'vmx_sat_unsigned':r'\buqadd\b',
        'vmx_sat_signed':r'\bsqadd\b',
        'vmx_compare':r'\b(cmgt|cmhi)\b',
        'vmx_convert_signed':r'\bfcvtzs\b',
        'vmx_convert_unsigned':r'\bfcvtzs\b',
        'vmx_uint_to_float':r'\b(scvtf|ucvtf)\b',
        'vmx_dot3':r'\bfmul\b',
        'vmx_dot4':r'\bfadd\b',
        'vmx_copy_unaligned':r'\bldr\s+q',
        'vmx_endian_shuffle':r'\b(tbl|rev32)\b',
    }
    checks={name:bool(re.search(pattern,bodies.get(name,''))) for name,pattern in expected.items()}
    checks['unaligned_vector_store']=bool(re.search(r'\bstr\s+q',bodies.get('vmx_copy_unaligned','')))
    checks['fpcr_read_write']=bool(re.search(r'\bmrs\b.*fpcr',text,re.I) and re.search(r'\bmsr\s+fpcr',text,re.I))
    return {'checks':checks,'passed':all(checks.values()),'note':'Object inspection supplements logged execution; not hardware evidence.'}

if __name__=='__main__':
    result=check(Path(sys.argv[1]).read_text());print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 5)
