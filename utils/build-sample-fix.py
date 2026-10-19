#!/usr/bin/env python3
"""Build the experimental sample fix with Quartus Lite and the existing BIOS."""

import argparse
import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RTL_DIRS = ['src/fpga/core/myc64-rtl', 'src/fpga/core/my1541-rtl']
GENERATED = [f'{RTL_DIRS[0]}/myc64.v', f'{RTL_DIRS[1]}/my1541.v']
MANIFEST = ROOT / 'output/sample-fix-inputs.json'


def hashes():
  paths = [ROOT / name for name in GENERATED]
  for directory in RTL_DIRS:
    paths.extend(sorted((ROOT / directory).rglob('*.py')))
  paths.append(ROOT / 'src/fpga/bios.vh')
  return {
      str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
      for path in paths
  }


def validate_rtl_interfaces():
  top = (ROOT / 'src/fpga/core/core_top.v').read_text()
  for filename, module, instance in zip(GENERATED, ['myc64_top', 'my1541_top'], ['u_myc64', 'u_my1541']):
    header = re.search(r'module\s+' + module + r'\s*\((.*?)\);', (ROOT / filename).read_text(), re.DOTALL)
    connection = re.search(module + r'\s+' + instance + r'\s*\((.*?)\);', top, re.DOTALL)
    if header is None or connection is None:
      raise RuntimeError(f'Missing generated interface for {module}')
    actual = {name.strip() for name in header.group(1).split(',')}
    expected = set(re.findall(r'\.(\w+)\s*\(', connection.group(1)))
    if expected - actual:
      raise RuntimeError(f'{module} is missing named ports: {sorted(expected - actual)}')


def prepare():
  subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'], cwd=ROOT, check=True)
  subprocess.run([sys.executable, str(ROOT / 'utils/build-bios-zig.py')], cwd=ROOT, check=True)
  for directory, script in zip(RTL_DIRS, ['myc64.py', 'my1541.py']):
    subprocess.run([sys.executable, script], cwd=ROOT / directory, check=True)
  validate_rtl_interfaces()
  MANIFEST.parent.mkdir(exist_ok=True)
  MANIFEST.write_text(json.dumps(hashes(), indent=2) + '\n')


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--prepare-only', action='store_true', help='Run HDL tests and generate Verilog; no Quartus needed')
  parser.add_argument('--prepared-rtl', action='store_true', help='Use hash-checked prepared Verilog; only Python stdlib needed')
  parser.add_argument('--quartus', help='Path to quartus_sh (otherwise found on PATH)')
  parser.add_argument('--version', default='0.2.7-sample-fix.1')
  args = parser.parse_args()
  if args.prepare_only and args.prepared_rtl:
    parser.error('--prepare-only and --prepared-rtl are mutually exclusive')
  quartus = shutil.which(args.quartus or 'quartus_sh')
  if not args.prepare_only and quartus is None:
    parser.exit(2, 'No bitstream built: quartus_sh is required (Quartus Lite 22.1 with Cyclone V support).\n')
  if args.prepared_rtl:
    if not MANIFEST.is_file() or json.loads(MANIFEST.read_text()) != hashes():
      parser.exit(2, 'Prepared RTL is missing or stale; run --prepare-only with requirements-dev.txt installed.\n')
  else:
    prepare()
  if args.prepare_only:
    print(f'RTL prepared and checked: {MANIFEST}')
    return
  validate_rtl_interfaces()
  for name in ['T65.vhd', 'T65_Pack.vhd', 'T65_MCode.vhd', 'T65_ALU.vhd']:
    if not (ROOT / RTL_DIRS[0] / 'T65' / name).is_file():
      parser.exit(2, 'Missing T65 sources; initialize only the T65 submodule, not the removed cpu-tv65 submodule.\n')

  rbf = ROOT / 'src/fpga/output_files/ap_core.rbf'
  # Never package a stale bitstream left by an earlier compile.
  rbf.unlink(missing_ok=True)
  subprocess.run([quartus, '--flow', 'compile', str(ROOT / 'src/fpga/ap_core.qpf')], cwd=ROOT, check=True)
  if not rbf.is_file() or rbf.stat().st_size == 0:
    raise RuntimeError('Quartus did not produce a bitstream')
  timing = ROOT / 'src/fpga/output_files/ap_core.sta.summary'
  slacks = re.findall(r'^\s*Slack\s*:\s*([-+]?\d+(?:\.\d+)?)', timing.read_text(), re.MULTILINE)
  if not slacks or any(float(slack) < 0 for slack in slacks):
    raise RuntimeError('Timing is missing or has negative slack; inspect ap_core.sta.summary before packaging')

  output = ROOT / 'output/MyC64-Pocket-sample-fix.zip'
  with tempfile.TemporaryDirectory(prefix='myc64-sample-fix-') as temp:
    stage = Path(temp) / 'release'
    shutil.copytree(ROOT / 'dist', stage)
    core = stage / 'Cores/markus-zzz.MyC64'
    subprocess.run([sys.executable, str(ROOT / 'utils/reverse-bits.py'), str(rbf), str(core / 'bitstream.rbf_r')],
                   check=True)
    config_path = core / 'core.json'
    config = json.loads(config_path.read_text())
    config['core']['metadata']['version'] = args.version
    config['core']['metadata']['date_release'] = datetime.date.today().isoformat()
    config_path.write_text(json.dumps(config, indent=4) + '\n')
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
      for path in sorted(stage.rglob('*')):
        if path.is_file() and path.name not in ['.gitkeep', '.keep']:
          archive.write(path, path.relative_to(stage))
  print(f'Experimental Pocket release: {output}')
  print('Not validated on physical Pocket hardware.')


if __name__ == '__main__':
  main()
