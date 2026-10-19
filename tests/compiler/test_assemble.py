"""Check the compiler's ordered-source assembly contract without compiling Rutile."""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
	"assemble", Path(__file__).resolve().parents[2] / "compiler" / "assemble.py"
)
module = importlib.util.module_from_spec(spec)
sys.dont_write_bytecode = True
spec.loader.exec_module(module)
temporary_root = Path(__file__).resolve().parents[2] / "build"
temporary_root.mkdir(exist_ok=True)


class AssemblyTests(unittest.TestCase):
	def test_order_bytes_and_unchanged_output(self):
		with tempfile.TemporaryDirectory(dir=temporary_root) as directory:
			root = Path(directory)
			(root / "src").mkdir()
			(root / "src" / "first.rt").write_bytes(b"first\r\n")
			(root / "src" / "second.rt").write_bytes(b"second\n")
			manifest = root / "sources.list"
			manifest.write_text("# explicit order\nsecond.rt\nfirst.rt\n")
			output = root / "build" / "compiler.rt"
			module.assemble(manifest, output)
			self.assertEqual(output.read_bytes(), b"second\nfirst\r\n")
			timestamp = output.stat().st_mtime_ns
			module.assemble(manifest, output)
			self.assertEqual(output.stat().st_mtime_ns, timestamp)
			(root / "src" / "second.rt").write_bytes(b"changed\n")
			module.assemble(manifest, output)
			self.assertEqual(output.read_bytes(), b"changed\nfirst\r\n")
			(root / "src" / "second.rt").write_bytes(b"no final newline")
			module.assemble(manifest, output)
			self.assertEqual(output.read_bytes(), b"no final newline\nfirst\r\n")

	def test_invalid_manifest_does_not_replace_output(self):
		with tempfile.TemporaryDirectory(dir=temporary_root) as directory:
			root = Path(directory)
			(root / "src").mkdir()
			(root / "src" / "first.rt").write_text("first\n")
			manifest = root / "sources.list"
			output = root / "compiler.rt"
			output.write_bytes(b"previous")
			for content in ("", "missing.rt\n", "first.rt\nfirst.rt\n", "../outside.rt\n"):
				with self.subTest(content=content):
					manifest.write_text(content)
					with self.assertRaises(ValueError):
						module.assemble(manifest, output)
					self.assertEqual(output.read_bytes(), b"previous")
			manifest.write_text("first.rt\n")
			for protected in (manifest, root / "src" / "first.rt"):
				with self.subTest(protected=protected):
					original = protected.read_bytes()
					with self.assertRaises(ValueError):
						module.assemble(manifest, protected)
					self.assertEqual(protected.read_bytes(), original)


if __name__ == "__main__":
	unittest.main()
