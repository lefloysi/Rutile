#include <cstdio>
#include <cstdlib>

extern "C" int rutile_backend_fixture(std::FILE* output, int optimization);

int main(int argc, char** argv) {
	if (argc != 3) {
		return 1;
	}
	auto output = std::fopen(argv[1], "wb");
	if (!output) {
		return 2;
	}
	return rutile_backend_fixture(output, std::atoi(argv[2]));
}
