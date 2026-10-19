using System;
using System.IO;
using System.Linq;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal static class ServerOutput {
		public static void Write(string message) {
			Console.Error.WriteLine(message);
		}
	}

	internal static class KilnWorkspaceTests {
		private static void Require(bool condition, string message) {
			if (!condition) {
				throw new InvalidOperationException(message);
			}
		}

		public static int Main(string[] arguments) {
			try {
				return Run(arguments);
			}
			catch (Exception error) {
				Console.Error.WriteLine(error);
				return 1;
			}
		}

		private static int Run(string[] arguments) {
			var root = arguments[0];
			Directory.CreateDirectory(Path.Combine(root, ".vs"));
			var cpp = Path.Combine(root, "CppProperties.json");
			File.WriteAllText(cpp, "{\"version\":4,\"configurations\":[{\"name\":\"User settings\",\"defines\":[\"KEEP\"]},{\"name\":\"Kiln: stale\"}]}");
			var description = new JObject {
				["targets"] = new JArray(
					new JObject {
						["name"] = "native app", ["compiler"] = "cpp", ["artifact"] = "C:/out/native.exe", ["executable"] = "C:/out/native.exe",
						["options"] = new JObject { ["includes"] = "[\"include dir\",\"C:/sdk/include\"]", ["defines"] = "ONE;TWO=2", ["standard"] = "c++23" }
					},
					new JObject { ["name"] = "module", ["compiler"] = "cpp", ["artifact"] = "C:/out/module.lib" },
					new JObject { ["name"] = "rt-app", ["compiler"] = "rutile", ["executable"] = "C:/out/rt-app.exe" }
				)
			};
			KilnWorkspaceFiles.Update(root, "C:/Program Files/Kiln/kiln.exe", "debug", description);
			var configurations = (JArray)JObject.Parse(File.ReadAllText(cpp))["configurations"]!;
			Require(configurations.Count == 3, "C++ target configurations were not generated.");
			Require(configurations[0].Value<string>("name") == "User settings", "User configuration was replaced.");
			Require(configurations[1]["defines"]!.Count() == 2, "Semicolon defines were not decoded.");
			Require(configurations[1]["includePath"]![0]!.ToString() == Path.Combine(root, "include dir"), "Relative include path was not resolved.");
			var tasks = (JArray)JObject.Parse(File.ReadAllText(Path.Combine(root, ".vs", "tasks.vs.json")))["tasks"]!;
			Require(tasks.Count == 3, "Build tasks were not created for every compiler.");
			Require(tasks[0]["args"]![1]!.ToString() == "native app", "Target arguments lost spaces.");
			var launches = (JArray)JObject.Parse(File.ReadAllText(Path.Combine(root, ".vs", "launch.vs.json")))["configurations"]!;
			Require(launches.Count == 2, "A library received a launch configuration.");
			KilnWorkspaceFiles.Update(root, "C:/Program Files/Kiln/kiln.exe", "release", description);
			configurations = (JArray)JObject.Parse(File.ReadAllText(cpp))["configurations"]!;
			Require(configurations.Count == 3 && configurations[1].Value<string>("name")!.Contains("release"), "Profile refresh accumulated stale entries.");
			Require(KilnProject.Quote("C:\\folder name\\") == "\"C:\\folder name\\\\\"", "Windows trailing-slash quoting failed.");
			Require(KilnProject.Quote("a\"b") == "\"a\\\"b\"", "Windows embedded-quote escaping failed.");
			File.WriteAllText(cpp, "invalid user content");
			try {
				KilnWorkspaceFiles.Update(root, "kiln.exe", "debug", description);
				throw new InvalidOperationException("Malformed user configuration should be preserved and reported.");
			}
			catch (Newtonsoft.Json.JsonException) {
				Require(File.ReadAllText(cpp) == "invalid user content", "Malformed user configuration was overwritten.");
			}
			Console.WriteLine("Kiln workspace configuration checks passed.");
			return 0;
		}
	}
}
