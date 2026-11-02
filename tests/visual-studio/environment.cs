using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using Newtonsoft.Json;
using Rutile.VisualStudio;

public static partial class EditorObservations {
	public static string SavedProjectEnvironment() {
		var values = new Dictionary<string, string> {
			["RUTILE_COMPILER:User"] = "user-compiler",
			["RUTILE_COMPILER:Machine"] = "machine-compiler",
			["KILN_DEPENDENCIES:Machine"] = "machine-dependencies",
			["VCPKG_ROOT:Process"] = "process-vcpkg",
			["VCPKG_ROOT:User"] = "user-vcpkg",
			["LLVM_ROOT:User"] = "user-llvm"
		};
		Func<string, EnvironmentVariableTarget, string?> read = (name, target) => values.TryGetValue(name + ":" + target, out var value) ? value : null;
		var environment = new StringDictionary { ["LLVM_ROOT"] = "explicit-child-llvm" };
		RutileProjectEnvironment.InheritConfiguration(environment, read);
		return JsonConvert.SerializeObject(new {
			user_compiler = environment["RUTILE_COMPILER"],
			machine_dependencies = environment["KILN_DEPENDENCIES"],
			process_vcpkg = environment["VCPKG_ROOT"],
			explicit_child_llvm = environment["LLVM_ROOT"],
			missing_executable = environment["KILN_EXECUTABLE"] == null,
			parent_unchanged = read("RUTILE_COMPILER", EnvironmentVariableTarget.Process) == null
		});
	}
}
