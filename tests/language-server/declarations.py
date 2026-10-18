"""Check declaration-first F12, declaration/definition toggling and type navigation."""

import argparse
from pathlib import Path
import tempfile

from protocol import Client, at, open_document, position, replace_document


def matches(locations, uri, source, name):
	expected = position(source, name)
	assert locations and all(item['uri'] == uri and item['range']['start'] == expected for item in locations), (locations, uri, expected)


def follow(client, location, method='textDocument/definition'):
	return client.request(method, {'textDocument': {'uri': location['uri']}, 'position': location['range']['start']})


def main(server):
	output = Path(__file__).resolve().parents[2] / 'build/language-server-tests'
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix='declaration navigation ', dir=output) as directory:
		root = Path(directory)
		declarations = 'struct Item;\nfn compute(i32 input)->i32;\n'
		definitions = 'struct Item {i32 value;};\nfn compute(i32 input)->i32 {return input;};\n'
		uses = 'using Alias=Item;\nfn main()->i32 {Item object;Item* pointer=&object;@object.value=42;return compute(pointer->value);};\n'
		first = (root / 'declarations.rt').as_uri()
		second = (root / 'definitions.rt').as_uri()
		third = (root / 'uses.rt').as_uri()
		client = Client(server, root)
		try:
			capabilities = client.request('initialize', {'rootUri': root.as_uri(), 'capabilities': {}})['capabilities']
			assert capabilities['typeDefinitionProvider'] is True
			open_document(client, first, declarations)
			open_document(client, second, definitions)
			open_document(client, third, uses)
			client.ready({first: 1, second: 1, third: 1})

			for name, use, declaration, definition in [
				('function', 'compute(pointer', 'compute(i32', 'compute(i32'),
				('type', 'Item object', 'Item;', 'Item {'),
			]:
				locations = client.request('textDocument/definition', at(third, uses, use))
				matches(locations, first, declarations, declaration)
				for _ in range(3):
					locations = follow(client, locations[0])
					matches(locations, second, definitions, definition)
					locations = follow(client, locations[0])
					matches(locations, first, declarations, declaration)
				print(name + ' use->declaration->definition->declaration: PASS', flush=True)

			for needle in ['object.value=42', 'pointer->value']:
				locations = client.request('textDocument/typeDefinition', at(third, uses, needle))
				matches(locations, first, declarations, 'Item;')
				locations = follow(client, locations[0], 'textDocument/typeDefinition')
				matches(locations, second, definitions, 'Item {')
			print('variable and pointer go to actual declared type: PASS', flush=True)

			# A symbol defined at its declaration must still give a useful target.
			field = client.request('textDocument/definition', at(third, uses, 'value=42'))
			matches(field, second, definitions, 'value;')
			matches(follow(client, field[0]), second, definitions, 'value;')
			alias = client.request('textDocument/definition', at(third, uses, 'Alias='))
			matches(alias, third, uses, 'Alias=')
			print('single-location fields and aliases retain their declaration: PASS', flush=True)

			# Moving a declaration must preserve toggling in the new snapshot.
			declarations = '\n' + declarations
			replace_document(client, first, declarations, 2)
			client.ready({first: 2, second: 1, third: 1})
			locations = client.request('textDocument/definition', at(second, definitions, 'compute(i32'))
			matches(locations, first, declarations, 'compute(i32')
			matches(follow(client, locations[0]), second, definitions, 'compute(i32')
			print('unsaved declaration edits preserve navigation targets: PASS', flush=True)
			client.close()
		finally:
			if client.process.poll() is None:
				client.process.kill()
				client.process.wait()


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--server', type=Path, default=Path('build/debug/bin/kiln.exe'))
	main(parser.parse_args().server.resolve())
