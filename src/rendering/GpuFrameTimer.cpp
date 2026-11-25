#include "src/app/AppState.h"
#include "src/rendering/GpuFrameTimer.h"

void beginGpuQuery()
{
	if (!renderMetrics::enabled) return;
	if (gpuQueries[0] == 0) glGenQueries(3, gpuQueries);
	const int slot = currentGpuQuery;
	if (gpuQueryPending[slot])
	{
		GLint disponible = GL_FALSE;
		glGetQueryObjectiv(gpuQueries[slot], GL_QUERY_RESULT_AVAILABLE, &disponible);
		if (disponible) { GLuint64 ns = 0; glGetQueryObjectui64v(gpuQueries[slot], GL_QUERY_RESULT, &ns); lastGpuMs = static_cast<double>(ns) / 1.0e6; gpuQueryPending[slot] = false; }
	}
	gpuQueryActive = !gpuQueryPending[slot];
	if (gpuQueryActive) glBeginQuery(GL_TIME_ELAPSED, gpuQueries[slot]);
}

void endGpuQuery()
{
	if (!renderMetrics::enabled || gpuQueries[0] == 0) return;
	if (gpuQueryActive) { glEndQuery(GL_TIME_ELAPSED); gpuQueryPending[currentGpuQuery] = true; }
	currentGpuQuery = (currentGpuQuery + 1) % 3;
}
