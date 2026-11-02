#!/usr/bin/env python3
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def include_roots(path, all_roots):
    roots = []
    compatibility = Path('csharp/compatibility_tests/v3.0.0/protos/src')
    if path.is_relative_to(compatibility):
        roots.append(compatibility)
    roots.extend([
        Path('src'),
        Path('java/core/src/main/resources'),
        Path('java/core/src/test/proto'),
        Path('.'),
        *path.parents,
        *all_roots,
    ])
    return list(dict.fromkeys(roots))


def generate(root, review, protoc, path, all_roots):
    includes = include_roots(path, all_roots)
    virtual = path
    for prefix in includes:
        if path.is_relative_to(prefix):
            virtual = path.relative_to(prefix)
            break
    flags = ['--experimental_editions']
    flags.extend('--proto_path=' + str(prefix) for prefix in includes)
    flags.append('--proto_path=third_party/golang/protobuf/v2/src=go')
    fixture_root = review.relative_to(root) / 'inputs'
    output_path = (
        Path('fixtures') / path.relative_to(fixture_root)
        if path.is_relative_to(fixture_root) else path
    )
    destination = review / 'java' / output_path.with_suffix('')
    if destination.exists():
        shutil.rmtree(destination)
    with tempfile.TemporaryDirectory(prefix='protoc-java-review-') as temporary:
        result = subprocess.run(
            [str(protoc), *flags, '--java_out=' + temporary, str(virtual)],
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=60,
        )
        if result.returncode not in (0, 1):
            raise subprocess.CalledProcessError(result.returncode, result.args, result.stdout)
        files = sorted(Path(temporary).rglob('*.java'))
        if result.returncode == 0 and files:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(temporary, destination)
        diagnostics = result.stdout.replace(str(root), '.').replace(temporary, '<output>')
    report = review / 'diagnostics' / path.with_suffix('.txt')
    if diagnostics:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(diagnostics)
    elif report.exists():
        report.unlink()
    return {
        'proto': str(path),
        'sha256': digest(root / path),
        'status': 'generated' if result.returncode == 0 else 'rejected',
        'java_files': len(files) if result.returncode == 0 else 0,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--protoc', required=True, type=Path)
    parser.add_argument('--generator-revision', required=True)
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    protoc = args.protoc.resolve(strict=True)
    review = Path(__file__).resolve().parent
    root = review.parent
    tracked = subprocess.check_output(
        ['git', 'ls-files', '-z', '*.proto'], cwd=root
    ).decode().split('\0')
    paths = sorted(
        Path(path) for path in tracked
        if path and not Path(path).is_relative_to(review.relative_to(root))
    )
    paths.append(review.relative_to(root) / 'inputs/generated_code_size_test.proto')
    paths.sort()
    all_roots = sorted(
        {parent for path in paths for parent in path.parents},
        key=lambda path: (len(path.parts), str(path)),
    )
    for directory in ('java', 'diagnostics'):
        destination = review / directory
        if destination.exists():
            shutil.rmtree(destination)
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(
            lambda path: generate(root, review, protoc, path, all_roots), paths
        ))
    revision = subprocess.check_output(
        ['git', 'rev-parse', args.generator_revision + '^{commit}'],
        cwd=root, text=True,
    ).strip()
    summary = {
        'generator_revision': revision,
        'protoc_version': subprocess.check_output([str(protoc), '--version'], text=True).strip(),
        'protoc_sha256': digest(protoc),
        'java_options': [],
        'experimental_editions': True,
        'tracked_proto_inputs': len(paths) - 1,
        'additional_generated_fixtures': 1,
        'generated_inputs': sum(row['status'] == 'generated' for row in results),
        'rejected_inputs': sum(row['status'] == 'rejected' for row in results),
        'java_files': sum(row['java_files'] for row in results),
        'inputs': results,
    }
    (review / 'manifest.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({key: value for key, value in summary.items() if key != 'inputs'}, indent=2))


if __name__ == '__main__':
    main()
