#include "src/app/AppState.h"
#include "src/rendering/GpuFrameTimer.h"

void iniciarConsultaGpu()
{
	if (!renderMetrics::enabled) return;
	if (consultasGpu[0] == 0) glGenQueries(3, consultasGpu);
	const int slot = consultaGpuActual;
	if (consultaGpuPendiente[slot])
	{
		GLint disponible = GL_FALSE;
		glGetQueryObjectiv(consultasGpu[slot], GL_QUERY_RESULT_AVAILABLE, &disponible);
		if (disponible) { GLuint64 ns = 0; glGetQueryObjectui64v(consultasGpu[slot], GL_QUERY_RESULT, &ns); ultimoGpuMs = static_cast<double>(ns) / 1.0e6; consultaGpuPendiente[slot] = false; }
	}
	consultaGpuActiva = !consultaGpuPendiente[slot];
	if (consultaGpuActiva) glBeginQuery(GL_TIME_ELAPSED, consultasGpu[slot]);
}

void finalizarConsultaGpu()
{
	if (!renderMetrics::enabled || consultasGpu[0] == 0) return;
	if (consultaGpuActiva) { glEndQuery(GL_TIME_ELAPSED); consultaGpuPendiente[consultaGpuActual] = true; }
	consultaGpuActual = (consultaGpuActual + 1) % 3;
}
