using System;
using System.IO;
using System.IO.Compression;

internal static class Pack {
	private static int Main(string[] args) {
		if (args.Length != 2) {
			Console.Error.WriteLine("Usage: Pack.exe PAYLOAD OUTPUT.zip");
			return 1;
		}
		try {
			if (File.Exists(args[1])) {
				File.Delete(args[1]);
			}
			ZipFile.CreateFromDirectory(args[0], args[1], CompressionLevel.Optimal, false);
			return 0;
		}
		catch (Exception error) {
			Console.Error.WriteLine(error.Message);
			return 1;
		}
	}
}
