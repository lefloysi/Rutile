#include "observations.hpp"

#include <Windows.h>
#include <metahost.h>
#include <cstdint>
#include <cstdio>
#include <string>

#pragma comment(lib, "mscoree.lib")
#pragma comment(lib, "ole32.lib")

const char* editor_observation_invoke(const char* assembly, const char* method) {
	const auto apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(apartment)) {
		return nullptr;
	}

	ICLRMetaHost* meta = nullptr;
	ICLRRuntimeInfo* runtime = nullptr;
	ICLRRuntimeHost* host = nullptr;
	char* output = nullptr;
	auto status = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&meta));
	if (SUCCEEDED(status)) {
		status = meta->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&runtime));
	}
	if (SUCCEEDED(status)) {
		status = runtime->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&host));
	}
	if (SUCCEEDED(status)) {
		status = host->Start();
	}
	if (SUCCEEDED(status)) {
		const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, assembly, -1, nullptr, 0);
		if (length) {
			std::wstring path(length, L'\0');
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, assembly, -1, path.data(), length);
			std::wstring argument = std::to_wstring(reinterpret_cast<uintptr_t>(&output)) + L":";
			while (*method) {
				argument += static_cast<unsigned char>(*method++);
			}
			DWORD result = 1;
			status = host->ExecuteInDefaultAppDomain(path.c_str(), L"EditorObservationEntry", L"Invoke", argument.c_str(), &result);
			if (result) {
				status = E_FAIL;
			}
		}
		else {
			status = E_INVALIDARG;
		}
	}
	if (FAILED(status)) {
		std::fprintf(stderr, "Editor observation failed: HRESULT 0x%08lx\n", static_cast<unsigned long>(status));
		CoTaskMemFree(output);
		output = nullptr;
	}
	if (host) {
		host->Release();
	}
	if (runtime) {
		runtime->Release();
	}
	if (meta) {
		meta->Release();
	}
	CoUninitialize();
	return output;
}

void editor_observation_release(const char* text) {
	CoTaskMemFree(const_cast<char*>(text));
}
