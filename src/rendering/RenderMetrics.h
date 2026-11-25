#pragma once
#include <array>
#include <algorithm>
#include <vector>
#include <GL/glew.h>

namespace renderMetrics
{
	inline bool enabled = false;
	inline unsigned long long drawCalls = 0, visibles = 0, descartados = 0;
	inline double oclusoresMs = 0.0, escenaMs = 0.0, fisicaMs = 0.0, animacionMs = 0.0, lucesMs = 0.0;
	inline double lastFrameMs = 0.0;
	inline std::vector<double> frameMs;
	inline void AddFrame(double ms) { lastFrameMs = ms; if (enabled) { frameMs.push_back(ms); if (frameMs.size() > 600) frameMs.erase(frameMs.begin(), frameMs.begin() + 120); } }
	inline double Percentile(double p)
	{
		if (frameMs.empty()) return 0.0;
		auto valores = frameMs; std::sort(valores.begin(), valores.end());
		return valores[static_cast<size_t>(p * (valores.size() - 1))];
	}
	inline void BeginRender() { drawCalls = visibles = descartados = 0; oclusoresMs = escenaMs = fisicaMs = animacionMs = lucesMs = 0.0; }
	inline void DrawArrays(GLenum mode, GLint first, GLsizei count) { if (enabled) ++drawCalls; glDrawArrays(mode, first, count); }
	inline void DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) { if (enabled) ++drawCalls; glDrawElements(mode, count, type, indices); }
}
