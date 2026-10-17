"""Assemble ordered Rutile modules for the single-source bootstrap compiler."""

import argparse
from pathlib import Path
import tempfile


def assemble(manifest: Path, output: Path) -> None:
	root = manifest.parent / "src"
	paths = []
	seen = set()
	for number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
		name = line.strip()
		if not name or name.startswith("#"):
			continue
		relative = Path(name)
		if relative.is_absolute() or ".." in relative.parts or relative.suffix != ".rt":
			raise ValueError(f"{manifest}:{number}: invalid source path: {name}")
		path = (root / relative).resolve()
		if not path.is_relative_to(root.resolve()):
			raise ValueError(f"{manifest}:{number}: source leaves module directory: {name}")
		if path in seen:
			raise ValueError(f"{manifest}:{number}: duplicate source: {name}")
		if not path.is_file():
			raise ValueError(f"{manifest}:{number}: missing source: {name}")
		seen.add(path)
		paths.append(path)
	if not paths:
		raise ValueError(f"{manifest}: no compiler sources")
	if output.resolve() == manifest.resolve() or output.resolve() in seen:
		raise ValueError(f"{output}: output would overwrite compiler source")
	chunks = []
	for path in paths:
		chunk = path.read_bytes()
		chunks.append(chunk if not chunk or chunk.endswith(b"\n") else chunk + b"\n")
	content = b"".join(chunks)
	output.parent.mkdir(parents=True, exist_ok=True)
	if not output.exists() or output.read_bytes() != content:
		with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
			temporary = Path(stream.name)
			try:
				stream.write(content)
				stream.close()
				temporary.replace(output)
			finally:
				temporary.unlink(missing_ok=True)


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--manifest", type=Path, required=True)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	try:
		assemble(args.manifest, args.output)
	except (OSError, ValueError) as error:
		parser.exit(1, f"{error}\n")
