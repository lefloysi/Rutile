using System;
using System.IO;
using System.Linq;
using Newtonsoft.Json.Linq;

namespace Rutile.VisualStudio {
	internal static class KilnWorkspaceFiles {
		public const string Prefix = "Kiln: ";

		public static JArray List(JToken? token) {
			var value = token?.ToString();
			if (string.IsNullOrWhiteSpace(value)) {
				return new JArray();
			}
			if (value!.TrimStart().StartsWith("[", StringComparison.Ordinal)) {
				return JArray.Parse(value);
			}
			return new JArray(value.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries));
		}

		public static void Merge(string path, string property, string identity, JArray generated, string version) {
			var document = File.Exists(path) ? JObject.Parse(File.ReadAllText(path)) : new JObject { ["version"] = version };
			if (version == "4" && !File.Exists(path)) {
				document["version"] = 4;
			}
			var items = document[property] as JArray ?? new JArray();
			foreach (var old in items.Where(item => item.Value<string>(identity)?.StartsWith(Prefix, StringComparison.Ordinal) == true).ToArray()) {
				old.Remove();
			}
			foreach (var item in generated) {
				items.Add(item);
			}
			document[property] = items;
			var text = document.ToString() + Environment.NewLine;
			if (File.Exists(path) && File.ReadAllText(path) == text) {
				return;
			}
			Directory.CreateDirectory(Path.GetDirectoryName(path)!);
			File.WriteAllText(path, text);
		}

		public static void Update(string root, string kiln, string profile, JObject description) {
			var tasks = new JArray();
			var launches = new JArray();
			var configurations = new JArray();
			foreach (var target in description["targets"] ?? new JArray()) {
				var name = target.Value<string>("name")!;
				var label = Prefix + name + " (" + profile + ")";
				var build = label + " build";
				tasks.Add(new JObject {
					["taskName"] = build,
					["appliesTo"] = "*",
					["type"] = "launch",
					["command"] = kiln,
					["args"] = new JArray("build", name, "--profile", profile),
					["workingDirectory"] = root,
					["contextType"] = "build",
					["output"] = target.Value<string>("artifact"),
					["problemMatcher"] = "$msCompile"
				});
				var executable = target.Value<string>("executable");
				if (!string.IsNullOrEmpty(executable)) {
					launches.Add(new JObject {
						["type"] = "default",
						["debugType"] = "native",
						["name"] = label,
						["project"] = executable,
						["projectTarget"] = "",
						["program"] = executable,
						["cwd"] = root,
						["currentDir"] = root,
						["args"] = new JArray()
					});
				}
				if (target.Value<string>("compiler") != "cpp") {
					continue;
				}
				var options = target["options"] as JObject ?? new JObject();
				var includes = List(options["includes"]);
				for (var index = 0; index < includes.Count; index++) {
					var include = includes[index].ToString();
					includes[index] = Path.IsPathRooted(include) ? include : Path.GetFullPath(Path.Combine(root, include));
				}
				configurations.Add(new JObject {
					["name"] = label,
					["inheritEnvironments"] = new JArray("msvc_x64"),
					["includePath"] = includes,
					["defines"] = List(options["defines"]),
					["forcedInclude"] = List(options["forced_includes"]),
					["intelliSenseMode"] = "windows-msvc-x64",
					["compilerSwitches"] = "/std:" + (options.Value<string>("standard") ?? "c++latest")
				});
			}
			Merge(Path.Combine(root, ".vs", "tasks.vs.json"), "tasks", "taskName", tasks, "0.2.1");
			Merge(Path.Combine(root, ".vs", "launch.vs.json"), "configurations", "name", launches, "0.2.1");
			Merge(Path.Combine(root, "CppProperties.json"), "configurations", "name", configurations, "4");
		}
	}
}
