"""Package tracked files at HEAD reproducibly; never include local build output."""
import argparse
import gzip
import hashlib
import io
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import zipfile


def git(*args):
    return subprocess.check_output(['git', *args])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    version = (root / 'source/release/VERSION').read_text().strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?', version):
        raise SystemExit('Invalid source release version')
    # Run from repository root so subdirectory invocation is safe.
    import os
    os.chdir(root)
    if subprocess.run(['git', 'diff', '--quiet', 'HEAD', '--']).returncode:
        raise SystemExit('Commit tracked source changes before packaging HEAD')
    subprocess.run([sys.executable, 'source/scripts/verify-version.py'], check=True)
    prefix = f'BSTY-FastSeed-v{version}'
    args.output.mkdir(parents=True, exist_ok=True)
    tar_bytes = git('archive', '--format=tar', f'--prefix={prefix}/', 'HEAD')
    tar_path = args.output / f'{prefix}.tar.gz'
    tar_path.write_bytes(gzip.compress(tar_bytes, mtime=0))
    zip_path = args.output / f'{prefix}.zip'
    zip_path.write_bytes(git('archive', '--format=zip', f'--prefix={prefix}/', 'HEAD'))
    # Compare both archive streams and verify every tracked-file payload.
    tracked = git('ls-tree', '-r', '--name-only', '-z', 'HEAD').decode().rstrip('\0').split('\0')
    with tarfile.open(fileobj=io.BytesIO(tar_bytes)) as tar, zipfile.ZipFile(zip_path) as archive:
        assert archive.testzip() is None
        tar_files = {m.name: tar.extractfile(m).read() for m in tar if m.isfile()}
        zip_files = {n: archive.read(n) for n in archive.namelist() if not n.endswith('/')}
        assert tar_files == zip_files
        assert set(tar_files) == {f'{prefix}/{p}' for p in tracked}
        for path in tracked:
            assert tar_files[f'{prefix}/{path}'] == git('show', f'HEAD:{path}')
    checksums = ''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
                        for p in [zip_path, tar_path])
    (args.output / 'SHA256SUMS.txt').write_text(checksums, encoding='utf-8', newline='\n')
    (args.output / 'BUILD-PROVENANCE.txt').write_text(
        f'Source commit: {git("rev-parse", "HEAD").decode().strip()}\n'
        'Tracked source files only; no prebuilt executable.\n'
        'Reboot recovery and live Tor/I2P: NOT TESTED.\n', encoding='utf-8')
    print(f'PASS: {len(tracked)} files match HEAD in ZIP and tar.gz')
    print(checksums)


if __name__ == '__main__':
    main()
