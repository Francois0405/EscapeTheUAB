#include "src/ui/SanityOverlay.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "src/gameplay/sanity/SanityEffects.h"

namespace runtimeui
{
namespace
{
using sanity::Clamp01;
using sanity::Hash01;

int Channel(float v) { return static_cast<int>(Clamp01(v) * 255.0f + 0.5f); }
ImU32 Rgba(float r, float g, float b, float a) { return IM_COL32(Channel(r), Channel(g), Channel(b), Channel(a)); }

// Hash por (semilla, paso de tiempo, canal): cambia a saltos, no cada fotograma, para que el glitch se lea.
float Noise(float seed, std::uint32_t step, std::uint32_t channel)
{
	return Hash01(static_cast<std::uint32_t>(seed * 977.0f) * 2654435761u + step * 40503u + channel * 9973u);
}

// Lineas finas de escaneo y una banda brillante que baja por la pantalla.
void DrawScanRays(ImDrawList* dl, const ImVec2& d, const sanity::SanityEffects& fx, float scale)
{
	const float i = fx.Insanity();
	if (i < 0.25f) return;
	const float t = sanity::Ramp(i, 0.25f, 1.0f);
	const float paso = std::max(3.0f, 3.0f * scale);
	const ImU32 linea = Rgba(0.0f, 0.0f, 0.0f, 0.05f + 0.16f * t);
	for (float y = 0.0f; y < d.y; y += paso)
		dl->AddRectFilled(ImVec2(0.0f, y), ImVec2(d.x, y + std::max(1.0f, scale)), linea);

	// Banda de barrido (una por pasada, mas rapida con menos cordura) con degradado vertical.
	const float velocidad = 0.18f + 0.5f * t;
	const float pos = std::fmod(fx.Time() * velocidad, 1.25f) - 0.1f;
	const float alto = (0.05f + 0.06f * t) * d.y, y = pos * d.y;
	const ImU32 fuerte = Rgba(0.35f, 1.0f, 0.55f, 0.05f + 0.16f * t), nulo = Rgba(0.35f, 1.0f, 0.55f, 0.0f);
	dl->AddRectFilledMultiColor(ImVec2(0.0f, y - alto), ImVec2(d.x, y), nulo, nulo, fuerte, fuerte);
	dl->AddRectFilledMultiColor(ImVec2(0.0f, y), ImVec2(d.x, y + alto * 0.35f), fuerte, fuerte, nulo, nulo);
}

void DrawGlitches(ImDrawList* dl, const ImVec2& d, const sanity::SanityEffects& fx, float scale)
{
	const std::uint32_t paso = static_cast<std::uint32_t>(fx.Time() * 24.0f);
	for (const sanity::Glitch& g : fx.Glitches())
	{
		const float u = g.age / g.life;
		const float env = 1.0f - u * u;
		const float sel = Noise(g.seed, 0, 1);
		const float cr = sel < 0.4f ? 0.1f : (sel < 0.7f ? 1.0f : (sel < 0.85f ? 0.0f : 0.9f));
		const float cg = sel < 0.4f ? 1.0f : (sel < 0.7f ? 0.05f : (sel < 0.85f ? 0.9f : 0.9f));
		const float cb = sel < 0.4f ? 0.3f : (sel < 0.7f ? 0.2f : (sel < 0.85f ? 1.0f : 0.9f));
		const float x0 = g.x * d.x + (Noise(g.seed, paso, 2) - 0.5f) * 60.0f * scale;
		const float x1 = x0 + g.width * d.x;
		const float y0 = g.y * d.y, y1 = y0 + std::max(g.height * d.y, scale);
		if (g.kind == 0)
		{
			dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), Rgba(cr, cg, cb, 0.10f + 0.20f * env));
			dl->AddRectFilled(ImVec2(x0 + 8.0f * scale, y0), ImVec2(x1, y0 + std::max(1.0f, scale)), Rgba(1, 1, 1, 0.35f * env));
		}
		else if (g.kind == 1)
		{
			// Rayo de escaneo: linea nitida a todo lo ancho con halo.
			dl->AddRectFilled(ImVec2(0.0f, y0 - 3.0f * scale), ImVec2(d.x, y0 + 3.0f * scale), Rgba(cr, cg, cb, 0.10f * env));
			dl->AddRectFilled(ImVec2(0.0f, y0), ImVec2(d.x, y0 + std::max(1.0f, scale)), Rgba(cr, cg, cb, 0.65f * env));
		}
		else
		{
			// Bloques de pixeles corruptos.
			const float celda = std::max(6.0f, 12.0f * scale);
			const int nx = std::max(1, static_cast<int>((x1 - x0) / celda)), ny = std::max(1, static_cast<int>((y1 - y0 + 4.0f * celda) / celda));
			for (int by = 0; by < std::min(ny, 6); ++by)
				for (int bx = 0; bx < std::min(nx, 12); ++bx)
				{
					const float r = Noise(g.seed, paso, 10u + static_cast<std::uint32_t>(by * 16 + bx));
					if (r < 0.45f) continue;
					dl->AddRectFilled(ImVec2(x0 + bx * celda, y0 + by * celda), ImVec2(x0 + (bx + 1) * celda, y0 + (by + 1) * celda),
						Rgba(r < 0.7f ? 0.0f : 1.0f, r < 0.85f ? 1.0f : 0.1f, r < 0.7f ? 0.4f : 0.9f, 0.25f * env));
				}
		}
	}
}

void DrawRain(ImDrawList* dl, const ImVec2& d, const sanity::SanityEffects& fx, float scale)
{
	static const char kChars[] = "01ABCDEF<>/\\|=+*#$%&?";
	const float tam = std::max(10.0f, 14.0f * scale);
	ImFont* fuente = ImGui::GetFont();
	const std::uint32_t paso = static_cast<std::uint32_t>(fx.Time() * 14.0f);
	for (const sanity::Rain& r : fx.RainColumns())
	{
		const float u = r.age / r.life;
		const float env = std::min(1.0f, std::min(u * 6.0f, (1.0f - u) * 3.0f));
		const float xc = r.x * d.x;
		const float yCabeza = (r.y0 + r.speed * r.age) * d.y;
		for (int k = 0; k < r.length; ++k)
		{
			const float y = yCabeza - k * tam;
			if (y < -tam || y > d.y) continue;
			const std::uint32_t h = static_cast<std::uint32_t>(Noise(r.seed, paso + static_cast<std::uint32_t>(k) * 3u, 5) * 1000.0f);
			const char c = kChars[h % (sizeof(kChars) - 1)];
			const float a = env * (k == 0 ? 0.85f : 0.55f * (1.0f - static_cast<float>(k) / r.length));
			dl->AddText(fuente, tam, ImVec2(xc, y), k == 0 ? Rgba(0.85f, 1.0f, 0.9f, a) : Rgba(0.1f, 1.0f, 0.35f, a), &c, &c + 1);
		}
	}
}

// Ojo en forma de almendra con parpado, iris que sigue el centro de la pantalla y venas rojas.
void DrawEye(ImDrawList* dl, const ImVec2& d, const sanity::Eye& e, const sanity::SanityEffects& fx, float scale)
{
	const float u = e.age / e.life;
	float env = std::min(1.0f, std::min(u * 5.0f, (1.0f - u) * 4.0f));
	const std::uint32_t paso = static_cast<std::uint32_t>(fx.Time() * 18.0f);
	// Parpadeo del propio ojo y parpadeo de señal: por momentos no se dibuja.
	if (Noise(e.seed, paso, 3) < 0.06f + 0.10f * fx.Insanity()) return;
	const float parpadeo = std::pow(std::max(0.0f, std::sin(e.age * 2.2f + e.seed)), 24.0f);
	const float abierto = std::max(0.08f, 1.0f - parpadeo);
	const float ancho = e.size * d.x * 0.5f, alto = ancho * 0.52f * abierto;
	const ImVec2 c(e.x * d.x + (Noise(e.seed, paso, 4) - 0.5f) * 6.0f * scale, e.y * d.y);

	constexpr int N = 14;
	ImVec2 arriba[N + 1], abajo[N + 1];
	for (int k = 0; k <= N; ++k)
	{
		const float t = static_cast<float>(k) / N, x = (t * 2.0f - 1.0f), curva = 1.0f - x * x;
		arriba[k] = ImVec2(c.x + x * ancho, c.y - curva * alto);
		abajo[k] = ImVec2(c.x + x * ancho, c.y + curva * alto * 0.85f);
	}
	std::vector<ImVec2> contorno;
	contorno.reserve(2 * N + 2);
	for (int k = 0; k <= N; ++k) contorno.push_back(arriba[k]);
	for (int k = N; k >= 0; --k) contorno.push_back(abajo[k]);
	dl->AddConvexPolyFilled(contorno.data(), static_cast<int>(contorno.size()), Rgba(0.93f, 0.85f, 0.82f, 0.85f * env));

	// Iris y pupila: miran hacia el jugador (centro de la pantalla) con pequeño temblor.
	const float dx = d.x * 0.5f - c.x, dy = d.y * 0.5f - c.y, dist = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
	const float radio = alto * 0.95f;
	const ImVec2 iris(c.x + dx / dist * ancho * 0.18f, c.y + dy / dist * alto * 0.2f);
	if (abierto > 0.3f)
	{
		dl->PushClipRect(ImVec2(c.x - ancho, c.y - alto), ImVec2(c.x + ancho, c.y + alto), true);
		dl->AddCircleFilled(iris, radio, Rgba(0.75f, 0.05f, 0.05f, 0.95f * env), 24);
		dl->AddCircleFilled(iris, radio * 0.45f, Rgba(0.0f, 0.0f, 0.0f, env), 20);
		dl->AddCircleFilled(ImVec2(iris.x - radio * 0.25f, iris.y - radio * 0.25f), radio * 0.12f, Rgba(1, 1, 1, 0.8f * env), 8);
		dl->PopClipRect();
	}
	for (int k = 1; k < N; k += 3)
	{
		const float s = Noise(e.seed, 0, 20u + static_cast<std::uint32_t>(k));
		dl->AddLine(arriba[k], ImVec2(Hash01(k) < 0.5f ? iris.x - radio : iris.x + radio, iris.y - radio * 0.3f * s),
			Rgba(0.7f, 0.0f, 0.05f, 0.5f * env), std::max(1.0f, scale * 0.8f));
	}
	dl->AddPolyline(arriba, N + 1, Rgba(0.05f, 0.0f, 0.0f, 0.95f * env), 0, std::max(1.5f, 2.0f * scale));
	dl->AddPolyline(abajo, N + 1, Rgba(0.05f, 0.0f, 0.0f, 0.95f * env), 0, std::max(1.5f, 2.0f * scale));
}

void DrawMessage(ImDrawList* dl, const ImVec2& d, const sanity::Message& m, const sanity::SanityEffects& fx, float scale)
{
	const float u = m.age / m.life;
	const float borde = std::min(u, 1.0f - u);
	const float env = std::min(1.0f, borde / 0.12f);
	// El glitch es mayor al aparecer y al desaparecer, y crece con la perdida de cordura.
	const float extremos = 1.0f - Clamp01(borde / 0.25f);
	const float g = Clamp01(0.2f + 0.7f * fx.Insanity() + 0.7f * extremos);
	const std::uint32_t paso = static_cast<std::uint32_t>(fx.Time() * (8.0f + 22.0f * fx.Insanity()));
	if (Noise(m.seed, paso, 1) < 0.10f * g + (u < 0.1f || u > 0.9f ? 0.2f : 0.0f)) return;   // parpadeo de señal

	std::string texto = sanity::kMessages[std::clamp(m.text, 0, sanity::kMessageCount - 1)];
	static const char kBasura[] = "#@%&$?!/\\|<>01";
	for (std::size_t k = 0; k < texto.size(); ++k)
		if (texto[k] != ' ' && Noise(m.seed, paso, 100u + static_cast<std::uint32_t>(k)) < 0.04f + 0.22f * g * g)
			texto[k] = kBasura[static_cast<std::size_t>(Noise(m.seed, paso, 300u + static_cast<std::uint32_t>(k)) * 1000.0f) % (sizeof(kBasura) - 1)];

	ImFont* fuente = ImGui::GetFont();
	const float tam = ImGui::GetFontSize() * m.scale * std::max(1.0f, scale);
	const ImVec2 dim = fuente->CalcTextSizeA(tam, 100000.0f, 0.0f, texto.c_str());
	float x = m.x * d.x - dim.x * 0.5f, y = m.y * d.y - dim.y * 0.5f;
	x = std::clamp(x, 6.0f, std::max(6.0f, d.x - dim.x - 6.0f));
	y = std::clamp(y, 6.0f, std::max(6.0f, d.y - dim.y - 6.0f));
	const float cromatica = (2.0f + 10.0f * g) * scale;

	constexpr int franjas = 3;
	for (int f = 0; f < franjas; ++f)
	{
		const float fy0 = y + dim.y * f / franjas, fy1 = y + dim.y * (f + 1) / franjas + 1.0f;
		const bool rota = Noise(m.seed, paso, 50u + static_cast<std::uint32_t>(f)) < g * 0.8f;
		const float salto = rota ? (Noise(m.seed, paso, 60u + static_cast<std::uint32_t>(f)) - 0.5f) * 70.0f * g * scale : 0.0f;
		dl->PushClipRect(ImVec2(0.0f, fy0), ImVec2(d.x, fy1), true);
		dl->AddText(fuente, tam, ImVec2(x + salto + 3.0f * scale, y + 3.0f * scale), Rgba(0, 0, 0, 0.7f * env), texto.c_str());
		dl->AddText(fuente, tam, ImVec2(x + salto - cromatica, y), Rgba(1.0f, 0.0f, 0.1f, 0.75f * env), texto.c_str());
		dl->AddText(fuente, tam, ImVec2(x + salto + cromatica, y), Rgba(0.0f, 0.9f, 1.0f, 0.55f * env), texto.c_str());
		dl->AddText(fuente, tam, ImVec2(x + salto, y), Rgba(1.0f, 0.88f, 0.88f, 0.95f * env), texto.c_str());
		dl->PopClipRect();
	}
}
}

void DrawSanityOverlay(const sanity::SanityEffects& effects, float uiScale)
{
	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f || effects.Insanity() < 0.2f) return;
	const float escala = std::max(0.5f, uiScale);
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	DrawScanRays(dl, io.DisplaySize, effects, escala);
	DrawRain(dl, io.DisplaySize, effects, escala);
	DrawGlitches(dl, io.DisplaySize, effects, escala);
	for (const sanity::Eye& e : effects.Eyes()) DrawEye(dl, io.DisplaySize, e, effects, escala);
	for (const sanity::Message& m : effects.Messages()) DrawMessage(dl, io.DisplaySize, m, effects, escala);
}
}
