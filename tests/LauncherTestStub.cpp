#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    const std::string executable = std::filesystem::path(argv[0]).filename().string();
    if (executable == "cmake.exe")
    {
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--build") return 1;
        return 0;
    }

    if (executable == "EntornVGI.exe")
    {
        const char* sentinel = std::getenv("ESCAPETHEUAB_LAUNCHER_TEST_SENTINEL");
        if (sentinel != nullptr)
        {
            std::ofstream marker(sentinel, std::ios::binary);
            marker << "stale executable launched";
        }
        return 0;
    }
    return 2;
}
