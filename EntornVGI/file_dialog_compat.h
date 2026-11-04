#pragma once

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(ENTORNVGI_USE_NFD_EXTENDED)
#include <nfd.h>
using nfdchar_t = nfdu8char_t;

inline nfdresult_t NFD_OpenDialogCompat(
	const nfdchar_t* filterList,
	const nfdchar_t* defaultPath,
	nfdchar_t** outPath)
{
	std::vector<std::string> specs;
	if (filterList != nullptr && filterList[0] != '\0')
	{
		std::string filters(filterList);
		std::size_t start = 0;
		while (start <= filters.size())
		{
			const std::size_t end = filters.find(',', start);
			std::string spec = filters.substr(start, end - start);
			const std::size_t first = spec.find_first_not_of(" .\t");
			const std::size_t last = spec.find_last_not_of(" \t");
			if (first != std::string::npos)
				specs.push_back(spec.substr(first, last - first + 1));
			if (end == std::string::npos)
				break;
			start = end + 1;
		}
	}

	std::vector<nfdu8filteritem_t> items;
	items.reserve(specs.size());
	for (const std::string& spec : specs)
		items.push_back({ "Supported files", spec.c_str() });

	nfdchar_t* selectedPath = nullptr;
	const nfdresult_t result = NFD_OpenDialogU8(
		&selectedPath,
		items.empty() ? nullptr : items.data(),
		static_cast<nfdfiltersize_t>(items.size()),
		defaultPath);
	if (result != NFD_OKAY)
		return result;

	const std::size_t pathLength = std::strlen(selectedPath);
	*outPath = static_cast<nfdchar_t*>(std::malloc(pathLength + 1));
	if (*outPath == nullptr)
	{
		NFD_FreePathU8(selectedPath);
		return NFD_ERROR;
	}
	std::memcpy(*outPath, selectedPath, pathLength + 1);
	NFD_FreePathU8(selectedPath);
	return NFD_OKAY;
}

inline bool NFD_InitCompat()
{
	return NFD_Init() == NFD_OKAY;
}

inline void NFD_QuitCompat()
{
	NFD_Quit();
}
#define NFD_OpenDialog(filterList, defaultPath, outPath) \
	NFD_OpenDialogCompat(filterList, defaultPath, outPath)
#else
#include "../include/ImGui/nfd.h"
inline bool NFD_InitCompat()
{
	return true;
}
inline void NFD_QuitCompat()
{
}
#endif
