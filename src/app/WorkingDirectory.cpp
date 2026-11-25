#include "src/app/AppState.h"
#include "src/app/WorkingDirectory.h"

// Locate copied resources beside the executable or in the source tree.
void SetWorkingDirectoryToAssetRoot(const char* executablePath)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	std::vector<fs::path> candidates;
	if (executablePath != nullptr && executablePath[0] != '\0')
	{
		fs::path executable = fs::weakly_canonical(fs::path(executablePath), ec);
		if (!ec)
		{
			candidates.push_back(executable.parent_path());
			candidates.push_back(executable.parent_path().parent_path());
			candidates.push_back(executable.parent_path().parent_path().parent_path());
			candidates.push_back(executable.parent_path().parent_path().parent_path().parent_path());
		}
	}
	candidates.push_back(fs::current_path(ec));

	for (const auto& candidate : candidates)
	{
		ec.clear();
		if (fs::exists(candidate / "assets" / "shaders" / "gouraud_shdrML.vert", ec) &&
			fs::exists(candidate / "assets" / "scenes" / "aula_informatica_calibrada.json", ec))
		{
			fs::current_path(candidate, ec);
			if (!ec)
				return;
		}
	}
}
