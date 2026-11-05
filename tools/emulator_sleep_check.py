#!/usr/bin/env python3
"""Verify live REST glyphs against the 5h53 render; use pebble-tool's Python."""
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageChops

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'shared-kit/tools'))
from emulator_smoke import run_smoke

if __name__=='__main__':
    result=run_smoke(ROOT)
    output=ROOT/'shots/emulator-smoke'
    expected=Image.open(ROOT/'shots/rest-5h53-detail.png').convert('RGB')
    actual=Image.open(output/'detail.png').convert('RGB')
    # Only the REST row: other live metrics, weather and clocks vary.
    row=(8,97,181,114)
    assert ImageChops.difference(expected.crop(row),actual.crop(row)).getbbox() is None, 'Live REST does not match REST 5H53'
    version=(ROOT/'shared-kit/VERSION').read_text().strip()
    assert result['report']['starts'] and all(v==version for v in result['report']['starts'])
    result['sleep_text_pixel_match']=True
    result['kit_revision']=subprocess.check_output(
        ['git','-C',str(ROOT/'shared-kit'),'rev-parse','HEAD'],text=True).strip()
    (output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Live REST 5H53 matches the deterministic render pixel for pixel.',flush=True)
