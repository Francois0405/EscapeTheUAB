#pragma once

namespace sanity { class SanityEffects; }

namespace runtimeui
{
	// Dibuja en la lista de primer plano de ImGui los efectos de baja cordura: rayos de escaneo, fallos de matrix,
	// lluvia de digitos, ojos y mensajes con glitch. No recibe raton ni toma el foco. uiScale: escala de la interfaz.
	void DrawSanityOverlay(const sanity::SanityEffects& effects, float uiScale);
}
