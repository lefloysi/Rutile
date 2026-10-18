"""Verify explicit semantic bootstrap for clients that bypass initialize middleware."""
import argparse
from pathlib import Path
import tempfile
from protocol import Client, open_document
from semantic_tokens import decode, token_at


def main(server):
	output = Path(__file__).resolve().parents[2] / 'build/language-server-tests'
	output.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix='semantic bootstrap ', dir=output) as temporary:
		root = Path(temporary)
		normal = Client(server, root)
		try:
			standard = normal.request('initialize', {'rootUri': root.as_uri(), 'capabilities': {}})
			legend = standard['capabilities']['semanticTokensProvider']['legend']
			assert normal.request('rutile/semanticTokensLegend', {}) == legend
		finally:
			normal.close()

		client = Client(server, root)
		try:
			custom = client.request('initialize', {'rootUri': root.as_uri(), 'capabilities': {}, 'initializationOptions': {'rutileSemanticColors': True}})
			assert 'semanticTokensProvider' not in custom['capabilities'], custom
			assert client.request('rutile/semanticTokensLegend', {}) == legend
			uri = (root / 'bootstrap.rt').as_uri()
			source = 'namespace rt {fn answer()->i32 {return 42;};};\nfn main()->i32 {return rt::answer();};\n'
			open_document(client, uri, source)
			client.ready({uri: 1})
			tokens = decode(client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}}), legend['tokenTypes'])
			token_at(tokens, source, 'answer()->', 'function')
			token_at(tokens, source, 'rt {', 'namespace')
			assert client.request('rutile/semanticTokensLegend', {}) == legend
		finally:
			client.close()
	print('Explicit semantic legend bootstrap, custom opt-in, standard clients and bound tokens: PASS')


if __name__ == '__main__':
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument('--server', type=Path, default=Path('build/debug/bin/kiln.exe'))
	main(parser.parse_args().server.resolve())
