#include "src/ui/RuntimeUi.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "src/audio/AudioManager.h"
#include "src/gameplay/player/FpsMovement.h"
#include "src/gameplay/sanity/Sanity.h"
#include "src/gameplay/sanity/SanityEffects.h"
#include "src/ui/SanityOverlay.h"
#include "src/physics/PhysicsScene.h"
#include "src/ui/RuntimeUiLogic.h"
#include "src/ui/UiScale.h"
#include "src/audio/SubtitleManager.h"
#include "src/rendering/RetroSettings.h"

namespace runtimeui
{
namespace
{
float uiScale = 1.0f;

// Barra de estamina: se dibuja en la lista de primer plano de ImGui (no es una ventana: no recibe raton ni toma el foco).
// Aparece al correr o mientras no esta llena y se desvanece sola. Solo se ve en la ventana, nunca dentro del visor VR.
void DrawStaminaBar(const HudContext& context)
{
	const auto& staminaBar = context.staminaBar;
	const auto& playerMovement = context.movement;
	if (!context.isFpsMode() || context.isEditorActive() || staminaBar.visible < 0.01f) return;
	const ImGuiIO& io = ImGui::GetIO();
	const float escala = uiScale;
	const float ancho = std::min(260.0f * escala, io.DisplaySize.x * 0.4f), alto = 8.0f * escala;
	const float x0 = (io.DisplaySize.x - ancho) * 0.5f, y0 = io.DisplaySize.y - 48.0f * escala;
	const float a = staminaBar.visible;
	const bool agotada = playerMovement.IsExhausted();
	const ImU32 fondo = IM_COL32(10, 10, 10, static_cast<int>(150 * a));
	const ImU32 relleno = agotada ? IM_COL32(200, 60, 50, static_cast<int>(230 * a))
		: IM_COL32(220, 220, 205, static_cast<int>(210 * a));
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRectFilled(ImVec2(x0 - 2 * escala, y0 - 2 * escala), ImVec2(x0 + ancho + 2 * escala, y0 + alto + 2 * escala), fondo, 3.0f * escala);
	dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + ancho * std::clamp(staminaBar.mostrada, 0.0f, 1.0f), y0 + alto), relleno, 2.0f * escala);
	if (context.showStaminaPercent)
	{
		char texto[16];
		std::snprintf(texto, sizeof(texto), "%d%%", static_cast<int>(playerMovement.StaminaFraction() * 100.0f + 0.5f));
		dl->AddText(ImVec2(x0 + ancho + 8 * escala, y0 - 4 * escala), IM_COL32(230, 230, 220, static_cast<int>(200 * a)), texto);
	}
}

// Tinte rojo por baja cordura: rectangulo a pantalla completa en la lista de fondo de ImGui (bajo el HUD y los
// menus, sobre la escena ya compuesta). Funciona con o sin efectos retro. Alfa 0 con cordura plena.
void DrawSanityTint(const HudContext& context)
{
	if (!context.isFpsMode() || context.isEditorActive()) return;
	const float alfa = context.sanitySystem.TintAlpha();
	if (alfa <= 0.001f) return;
	const ImGuiIO& io = ImGui::GetIO();
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize,
		IM_COL32(170, 0, 8, static_cast<int>(std::clamp(alfa, 0.0f, 1.0f) * 255.0f + 0.5f)));
}

void DrawSanityEffects(const HudContext& context)
{
	if (!context.isFpsMode() || context.isEditorActive()) return;
	DrawSanityOverlay(context.sanityEffects, uiScale);
}

// Barra de cordura abajo a la derecha, anclada al borde de la ventana y escalada con uiScale.
// Se encoge de derecha a izquierda y cambia de color de forma continua (rosa -> magenta -> rojo).
void DrawSanityBar(const HudContext& context)
{
	if (!context.isFpsMode() || context.isEditorActive()) return;
	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
	const float escala = uiScale;
	const float margen = 24.0f * escala;
	const float ancho = std::min(220.0f * escala, io.DisplaySize.x * 0.35f), alto = 14.0f * escala;
	const float x1 = io.DisplaySize.x - margen, x0 = x1 - ancho;
	const float y1 = io.DisplaySize.y - margen, y0 = y1 - alto;
	const float fraccion = context.sanitySystem.Fraction();
	const sanity::Color c = sanity::BarColor(fraccion);
	const auto canal = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRectFilled(ImVec2(x0 - 2 * escala, y0 - 2 * escala), ImVec2(x1 + 2 * escala, y1 + 2 * escala),
		IM_COL32(10, 10, 10, 170), 3.0f * escala);
	if (fraccion > 0.0f)
		dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + ancho * fraccion, y1),
			IM_COL32(canal(c.r), canal(c.g), canal(c.b), 235), 2.0f * escala);
	char texto[24];
	std::snprintf(texto, sizeof(texto), "CORDURA %d%%", static_cast<int>(fraccion * 100.0f + 0.5f));
	const ImVec2 tam = ImGui::CalcTextSize(texto);
	dl->AddText(ImVec2(x1 - tam.x, y0 - tam.y - 5 * escala), IM_COL32(canal(c.r), canal(c.g), canal(c.b), 230), texto);
}

void DrawInteractionHud(const HudContext& context)
{
	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;
	const bool agarrado = context.playerHoldsObject && context.playerHoldsObject();
	const ReticleState estado = EvaluateReticle(context.isDesktopHudVisible && context.isDesktopHudVisible(),
		io.WantTextInput || io.WantCaptureKeyboard || io.WantCaptureMouse,
		agarrado, context.throwCharging, context.throwChargeSeconds);
	if (!estado.reticle) return;

	const float escala = uiScale;
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	const ImVec2 centro(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
	// Cuadrado de 5 unidades con contorno oscuro de 1: sigue visible sobre fondos claros.
	const float medio = 2.5f * escala;
	dl->AddRectFilled(ImVec2(centro.x - medio - escala, centro.y - medio - escala),
		ImVec2(centro.x + medio + escala, centro.y + medio + escala), IM_COL32(0, 0, 0, 170));
	dl->AddRectFilled(ImVec2(centro.x - medio, centro.y - medio),
		ImVec2(centro.x + medio, centro.y + medio), IM_COL32(255, 250, 235, 245));
	if (estado.arc)
	{
		// Bloques alineados a pixeles fisicos, sin antialiasing; empieza arriba y avanza en horario.
		const float densidad = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f;
		const int celda = ChargeCellPixels(escala, densidad);
		const int borde = std::max(1, static_cast<int>(std::lround(escala * densidad)));
		const int origenX = static_cast<int>(std::floor(centro.x * densidad)) - celda / 2;
		const int origenY = static_cast<int>(std::floor(centro.y * densidad)) - celda / 2;
		const ImU32 color = estado.full ? IM_COL32(0xF0, 0xCA, 0x65, 255) : IM_COL32(255, 248, 230, 255);
		const int encendidos = LitChargeSegments(estado.fraction, estado.full);
		// Primero todos los contornos y despues los rellenos, para que un contorno no tape un vecino.
		for (int pasada = 0; pasada < 2; ++pasada)
			for (int i = 0; i < encendidos; ++i)
			{
				const Cell c = ChargeSegmentCell(i);
				const int margen = pasada == 0 ? borde : 0;
				const int x = origenX + c.x * celda, y = origenY + c.y * celda;
				dl->AddRectFilled(ImVec2((x - margen) / densidad, (y - margen) / densidad),
					ImVec2((x + celda + margen) / densidad, (y + celda + margen) / densidad),
					pasada == 0 ? IM_COL32(0, 0, 0, 170) : color);
			}
	}

	// Aviso breve al pasar a sostener un objeto, abajo a la izquierda.
	static bool agarradoAntes = false;
	static double avisoHasta = 0.0;
	if (agarrado && !agarradoAntes) avisoHasta = ImGui::GetTime() + 1.6;
	agarradoAntes = agarrado;
	if (context.showPickupNotice && agarrado && ImGui::GetTime() < avisoHasta)
	{
		const ImVec2 pos(18.0f * escala, io.DisplaySize.y - 92.0f * escala);
		dl->AddText(ImVec2(pos.x + escala, pos.y + escala), IM_COL32(0, 0, 0, 230), "Objeto agarrado");
		dl->AddText(pos, IM_COL32(0xF0, 0xCA, 0x65, 255), "Objeto agarrado");
	}

	if (agarrado)
	{
		// Abajo a la derecha, lejos de la mira y de los subtitulos.
		const char* ayuda = "Mantener E carga el lanzamiento; soltar E lanza";
		const ImVec2 medida = ImGui::CalcTextSize(ayuda);
		const ImVec2 pos(io.DisplaySize.x - medida.x - 18.0f * escala, io.DisplaySize.y - medida.y - 14.0f * escala);
		dl->AddText(ImVec2(pos.x + escala, pos.y + escala), IM_COL32(0, 0, 0, 230), ayuda);
		dl->AddText(pos, IM_COL32(245, 248, 240, 255), ayuda);
		return;
	}
	const char* prompt = context.interactionPrompt();
	if (prompt == nullptr) return;

	const float tamanoIcono = 34.0f * escala;
	const float x = (io.DisplaySize.x - tamanoIcono) * 0.5f;
	const float y = std::min(io.DisplaySize.y * 0.5f + 24.0f * escala, io.DisplaySize.y - tamanoIcono - 12.0f * escala);
	if (context.handTexture != 0)
	{
		const ImTextureID textura = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(context.handTexture));
		dl->AddImage(textura, ImVec2(x, y), ImVec2(x + tamanoIcono, y + tamanoIcono),
			ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f));
	}
	const ImU32 sombra = IM_COL32(0, 0, 0, 220);
	dl->AddText(ImVec2(x + tamanoIcono + 7.0f * escala, y + 11.0f * escala), sombra, prompt);
	dl->AddText(ImVec2(x + tamanoIcono + 6.0f * escala, y + 10.0f * escala), IM_COL32(245, 248, 240, 255), prompt);
}

// Escala UI en unidades lógicas de GLFW. La resolución lógica y el DPI determinan el diseño;
// el framebuffer nativo solo aporta densidad de píxel a ImGui y el FBO PS1 solo dibuja la escena.
float UpdateUiScale(GLFWwindow* ventana)
{
	int anchoLogico = 0, altoLogico = 0;
	float escalaDpiX = 1.0f, escalaDpiY = 1.0f;
	glfwGetWindowSize(ventana, &anchoLogico, &altoLogico);
	glfwGetWindowContentScale(ventana, &escalaDpiX, &escalaDpiY);

	// Media geométrica: combina densidad y espacio lógico sin duplicar el factor de framebuffer.
	uiScale = uiscale::ComputeUiScale(
		static_cast<float>(anchoLogico), static_cast<float>(altoLogico), escalaDpiX, escalaDpiY);

	// ImGui GLFW conserva DisplaySize en unidades lógicas y calcula DisplayFramebufferScale
	// desde el framebuffer nativo; ninguno de esos valores se sustituye por el FBO PS1 de la escena.
	return uiScale;
}

void DrawVolumeControl(const MenuContext& context)
{
	ImGui::SetNextItemWidth(240.0f * uiScale);
	if (ImGui::SliderFloat("Volumen", &context.masterVolumePercent, 0.0f, 100.0f, "%.0f%%"))
		context.audio.setMasterVolume(context.masterVolumePercent / 100.0f);
}

void DrawMenus(const MenuContext& context)
{
	// =========================================================================
	// MENÚ PRINCIPAL
	// =========================================================================
	if (context.state == GAME_STATE_MAIN_MENU)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		// 1. Ocupar el 100% de la ventana desde la esquina superior izquierda
		ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		                         ImGuiWindowFlags_NoResize |
		                         ImGuiWindowFlags_NoMove |
		                         ImGuiWindowFlags_NoCollapse |
		                         ImGuiWindowFlags_NoScrollbar |
		                         ImGuiWindowFlags_NoScrollWithMouse;

		// 2. Quitar márgenes interiores (WindowPadding) y bordes para que no quede un marco alrededor de la foto
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.05f, 0.05f, 0.08f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.14f, 0.20f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.30f, 0.42f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.96f, 0.97f, 1.00f, 1.00f));

		ImGui::Begin("MenuPrincipal", nullptr, flags);

		// 3. Dibujar tu foto ocupando toda la pantalla
		if (context.backgroundTexture != 0)
		{
			ImGui::Image((ImTextureID)(intptr_t)context.backgroundTexture, viewport->Size);
		}

		// 4. Posicionar título y botones ENCIMA de la foto
		float anchoBoton = 220.0f * uiScale;
		float altoBoton  = 45.0f * uiScale;
		float centroX    = viewport->Size.x * 0.5f;
		float inicioY    = viewport->Size.y * 0.50f; // 0.50f = mitad vertical de la pantalla

		const char* titulo = "ESCAPE THE UAB";
		ImGui::SetCursorPos(ImVec2(centroX - ImGui::CalcTextSize(titulo).x * 0.5f, inicioY - 60.0f * uiScale));
		ImGui::Text("%s", titulo);

		ImGui::SetCursorPos(ImVec2(centroX - anchoBoton * 0.5f, inicioY));
		if (ImGui::Button("JUGAR", ImVec2(anchoBoton, altoBoton)))
		{
			context.state = GAME_STATE_PLAYING;
			context.resetMouseLook();
		}

		ImGui::SetCursorPos(ImVec2(centroX - anchoBoton * 0.5f, inicioY + altoBoton + 15.0f * uiScale));
		if (ImGui::Button("SALIR", ImVec2(anchoBoton, altoBoton)))
		{
			glfwSetWindowShouldClose(context.window, true);
		}

		ImGui::SetCursorPos(ImVec2(centroX - 120.0f * uiScale, inicioY + altoBoton * 2.0f + 45.0f * uiScale));
		DrawVolumeControl(context);

		ImGui::End();
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(4);
	}
	// =========================================================================
	// MENÚ DE PAUSA
	// =========================================================================
	else if (context.state == GAME_STATE_PAUSED)
	{
		ImVec2 centro = ImGui::GetMainViewport()->GetCenter();
		float ancho = 300.0f * uiScale;
		float alto  = 275.0f * uiScale;

		ImGui::SetNextWindowPos(centro, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(ancho, alto));

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
		                         ImGuiWindowFlags_NoResize |
		                         ImGuiWindowFlags_NoMove |
		                         ImGuiWindowFlags_NoCollapse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.88f));
		ImGui::PushStyleColor(ImGuiCol_Button,   ImVec4(0.20f, 0.24f, 0.32f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_Text,     ImVec4(0.96f, 0.97f, 1.00f, 1.00f));

		ImGui::Begin("MenuPausa", nullptr, flags);

		const char* titulo = "PAUSA";
		ImGui::SetCursorPosX((ancho - ImGui::CalcTextSize(titulo).x) * 0.5f);
		ImGui::Text("%s", titulo);

		ImGui::Dummy(ImVec2(0.0f, 25.0f * uiScale));

		float anchoBoton = 200.0f * uiScale;
		float altoBoton  = 40.0f * uiScale;

		ImGui::SetCursorPosX((ancho - anchoBoton) * 0.5f);
		// ---> AQUÍ VA EL AJUSTE DEL BOTÓN REANUDAR <---
		if (ImGui::Button("REANUDAR", ImVec2(anchoBoton, altoBoton)))
		{
			context.state = GAME_STATE_PLAYING;
			context.resetMouseLook();
		}

		ImGui::Dummy(ImVec2(0.0f, 12.0f * uiScale));

		ImGui::SetCursorPosX((ancho - anchoBoton) * 0.5f);
		if (ImGui::Button("MENU PRINCIPAL", ImVec2(anchoBoton, altoBoton)))
		{
			context.state = GAME_STATE_MAIN_MENU;
		}

		ImGui::Dummy(ImVec2(0.0f, 12.0f * uiScale));
		ImGui::SetCursorPosX((ancho - 240.0f * uiScale) * 0.5f);
		DrawVolumeControl(context);

		ImGui::End();
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
	}
}
}

// Franja inferior central: amarillo calido sobre negro al 72 %, con la fuente pixelada de ImGui.
static void DrawSubtitles(const SubtitleContext& context)
{
	// Se consulta siempre para olvidar lo que ya no suena aunque esten desactivados.
	subtitles::Visible linea;
	const bool hay = context.manager.Current(context.isPlaying, context.cursorSeconds,
		{ context.settings.soundCaptions, context.settings.unverifiedSubtitles }, linea);
	if (!hay || !context.settings.subtitles) return;
	const ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f) return;

	std::string texto = linea.speaker.empty() ? linea.text : linea.speaker + ": " + linea.text;
	if (!linea.verified) texto += "  (sin verificar)";
	ImFont* fuente = ImGui::GetFont();
	// 26 px a 1080p; ProggyClean mide 13 px, asi que el doble mantiene los pixeles nitidos.
	const float tamano = std::max(13.0f, 26.0f * io.DisplaySize.y / 1080.0f);
	const float ajuste = io.DisplaySize.x * 0.72f;
	const ImVec2 medida = fuente->CalcTextSizeA(tamano, FLT_MAX, ajuste, texto.c_str());
	const float margen = tamano * 0.35f;
	const ImVec2 pos(std::floor((io.DisplaySize.x - medida.x) * 0.5f),
		std::floor(io.DisplaySize.y * 0.92f - medida.y - margen * 0.5f));
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRectFilled(ImVec2(pos.x - margen, pos.y - margen * 0.5f),
		ImVec2(pos.x + medida.x + margen, pos.y + medida.y + margen * 0.5f), IM_COL32(0, 0, 0, 184));
	dl->AddText(fuente, tamano, pos, IM_COL32(0xF0, 0xCA, 0x65, 255), texto.c_str(), nullptr, ajuste);
}

void DrawRuntimeUiFrame(const FrameContext& context)
{
	static ImGuiStyle estiloUIBase;
	static bool estiloUIBaseCapturado = false;

	UpdateUiScale(context.menu.window);
	if (!estiloUIBaseCapturado)
	{
		estiloUIBase = ImGui::GetStyle();
		estiloUIBaseCapturado = true;
	}
	// Restaurar desde el estilo sin escalar impide acumular el factor al redimensionar o cambiar de monitor.
	ImGui::GetStyle() = estiloUIBase;
	if (context.useLightStyle)
		ImGui::StyleColorsLight();
	else
		ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(uiScale);
	ImGui::GetIO().FontGlobalScale = uiScale;

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// 1. La barra de estamina solo se dibuja mientras juegas
	if (context.menu.state == GAME_STATE_PLAYING)
	{
		DrawSanityTint(context.hud);
		DrawSanityEffects(context.hud);
		DrawStaminaBar(context.hud);
		DrawSanityBar(context.hud);
		DrawInteractionHud(context.hud);
	}

	// El audio sigue sonando en pausa y en el menu; el menu principal no muestra subtitulos.
	if (context.menu.state != GAME_STATE_MAIN_MENU) DrawSubtitles(context.subtitle);

	DrawMenus(context.menu);

	if (context.network.panelVisible)
	{
		DrawMultiplayerPanel(context.network, uiScale);
		if (context.hud.isFpsMode()) DrawDynamicObjectsPanel(context.dynamicObjects);
	}

	context.drawLegacyWindows();
	if (context.hud.isFpsMode() && context.showDebugPanels) DrawDebugPanels(context.debug);

	context.drawEditor(static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));

	if (context.hud.isFpsMode() && context.showFpsCounter)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x + viewport->Size.x - 12.0f, viewport->Pos.y + 12.0f),
			ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.7f);
		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
		if (ImGui::Begin("FPS Overlay", nullptr, flags))
			ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
		ImGui::End();
	}

	// Rendering ImGui Menus (src/app submits the draw data after the PS1 scene pass).
	ImGui::Render();
}
}
