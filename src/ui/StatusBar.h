#pragma once

#include <GL/glew.h>   // GL scalar types used by constants.h
#include <string>
#include "constants.h"

namespace runtimeui
{
	// Snapshot of the legacy VGI view values printed by the console status bar.
	// Member order is part of the contract with FormatStatusBar's structured binding.
	struct StatusBarState
	{
		std::string fileName;
		char projection;
		char camera;
		char object;
		char polarAxis;
		CEsfe3D sphericalView;
		CEsfe3D geodeView;
		CPunt3D navigationOrigin;
		bool pan;
		CPunt3D panTranslation;
		bool gridMode;
		bool objectColorMode;
		bool transformMode;
		bool rotate;
		bool translate;
		bool scale;
		bool backgroundRed;
		bool backgroundGreen;
		bool backgroundBlue;
		INSTANCIA transform;
		CColor backgroundColor;
		CColor objectColor;
		int fractalStep;
	};

	std::string FormatStatusBar(const StatusBarState& state);
}
