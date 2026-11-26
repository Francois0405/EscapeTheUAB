#include "src/ui/RuntimeUiLogic.h"
#include "src/ui/StatusBar.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
	int failures = 0;

	void Check(bool condition, const char* what)
	{
		if (condition) return;
		std::cerr << "FAIL: " << what << '\n';
		++failures;
	}

	bool Contains(const std::string& text, const std::string& part)
	{
		return text.find(part) != std::string::npos;
	}

	runtimeui::StatusBarState BaseState()
	{
		runtimeui::StatusBarState state{};
		state.projection = PERSPECT;
		state.camera = CAM_ESFERICA;
		state.object = CUB;
		state.polarAxis = POLARZ;
		state.sphericalView = { 10.0, 0.0, 0.0 };
		state.geodeView = { 5.0, 0.0, 0.0 };
		state.navigationOrigin = { 3.0, 4.0, 0.0, 1.0 };
		return state;
	}

	// Golden text follows Barra_Estat in EntornVGI/main.cpp at 852920c (lines 3112-3320). A static diff of
	// that body against FormatStatusBar differs only in fprintf(stderr, ...) -> Append(output, ...)
	// and one trailing blank line, so every literal, space and format is the original one.
	void TestStatusBar()
	{
		auto state = BaseState();
		Check(runtimeui::FormatStatusBar(state) ==
			"R=: 10.000000 a=: 0.000000 \xC3\x9F=: 0.000000  "
			"PVx= 10.000000 PVy= 0.000000 PVz= 0.000000 \n"
			"FONS     0.000000 0.000000 0.000000 \n"
			"           \n", "spherical camera full output");

		state.projection = CAP;
		Check(Contains(runtimeui::FormatStatusBar(state), "R=:         a=:        "), "no projection blanks");
		state.projection = ORTO;
		Check(Contains(runtimeui::FormatStatusBar(state), "R=:  ORTO    a=: ORTO   "), "orthographic labels");

		state = BaseState();
		state.camera = CAM_NAVEGA;
		Check(Contains(runtimeui::FormatStatusBar(state), "R=:  NAV   "), "navigation label");
		Check(Contains(runtimeui::FormatStatusBar(state), "PVx= 3.000000 PVy= 4.000000"), "navigation origin");

		state = BaseState();
		state.camera = CAM_GEODE;
		Check(Contains(runtimeui::FormatStatusBar(state), "PVx= 6.000000"), "geode adds the view direction");

		state = BaseState();
		state.polarAxis = POLARY;
		Check(Contains(runtimeui::FormatStatusBar(state), "PVx= 0.000000 PVy= 0.000000 PVz= 10.000000"), "POLARY axes");
		state.polarAxis = POLARX;
		Check(Contains(runtimeui::FormatStatusBar(state), "PVx= 0.000000 PVy= 10.000000 PVz= 0.000000"), "POLARX axes");

		state = BaseState();
		state.pan = true;
		state.panTranslation = { 1.0, 2.0, 3.0, 0.0 };
		Check(Contains(runtimeui::FormatStatusBar(state), "PVx= 1.000000 PVy= 2.000000 PVz= 3.000000 \nPAN "), "pan values");

		state = BaseState();
		state.transformMode = true;
		state.scale = true;
		state.transform.VScal = { 2.0, 3.0, 4.0, 0.0 };
		// The third component has always repeated X; the extraction keeps that output.
		Check(Contains(runtimeui::FormatStatusBar(state), "ESC  2.000000 3.000000 2.000000 \n"), "scale values");

		state = BaseState();
		state.backgroundRed = state.backgroundBlue = true;
		state.objectColorMode = true;
		state.objectColor = { 0.5, 0.25, 1.0, 1.0 };
		Check(Contains(runtimeui::FormatStatusBar(state), "OBJ   R   B  0.500000 0.250000 1.000000 \n"), "object colour");

		state = BaseState();
		state.fileName = "escena.obj";
		state.object = O_FRACTAL;
		state.fractalStep = 7;
		const std::string fractal = runtimeui::FormatStatusBar(state);
		Check(fractal.rfind("Fitxer: escena.obj \n", 0) == 0, "file name line");
		Check(Contains(fractal, "Pas= 7 \n"), "fractal step");
	}

	void TestNetworkStartRequest()
	{
		netgame::NetworkConfig config;
		std::string error;
		Check(runtimeui::BuildNetworkStartRequest(1, "", "ignored", 27015, config, error) &&
			config.rol == net::Role::Host && config.puerto == 27015 && config.nombre == "jugador" && error.empty(),
			"host request with default name");

		config = {};
		Check(runtimeui::BuildNetworkStartRequest(2, "ana", "192.168.1.20", 4000, config, error) &&
			config.rol == net::Role::Client && config.host == "192.168.1.20" && config.puerto == 4000 && config.nombre == "ana",
			"client request");

		config = {};
		Check(!runtimeui::BuildNetworkStartRequest(1, "ana", "", 70000, config, error) &&
			error == "Puerto fuera de rango: usa un valor entre 1 y 65535.", "port out of range");

		config = {};
		error.clear();
		Check(!runtimeui::BuildNetworkStartRequest(2, "ana", "999.1.1.1", 4000, config, error) && !error.empty(),
			"invalid client address");
	}

	struct Row
	{
		std::string id;
		bool sujeto = false;
		bool activo = false;
	};

	void TestDynamicRows()
	{
		std::vector<Row> rows{ {"c"}, {"b", false, true}, {"z", true, false}, {"a"} };
		runtimeui::OrderAndLimitDynamicRows(rows);
		Check(rows.size() == 4 && rows[0].id == "z" && rows[1].id == "b" && rows[2].id == "a" && rows[3].id == "c",
			"held, active, then id");

		rows.assign(12, Row{ "x" });
		runtimeui::OrderAndLimitDynamicRows(rows);
		Check(rows.size() == 8, "table limited to 8 rows");

		Check(std::string(runtimeui::HiritiFollowStatus(true, true, true)) == " | Hiriti: seguimiento autoritativo desconocido (host)", "client status");
		Check(std::string(runtimeui::HiritiFollowStatus(false, false, true)) == " | Hiriti: seguimiento local inactivo", "inactive status");
		Check(std::string(runtimeui::HiritiFollowStatus(false, true, true)) == " | Hiriti: seguimiento local suspendido", "suspended status");
		Check(std::string(runtimeui::HiritiFollowStatus(false, true, false)) == " | Hiriti: seguimiento local elegible", "eligible status");
	}
}

// Reticula y anillo de carga: visibilidad, progreso, lanzamiento y cancelacion.
void TestReticle()
{
	using runtimeui::EvaluateReticle;
	Check(!EvaluateReticle(false, false, true, true, 1.0f).reticle, "hidden outside desktop gameplay");
	Check(!EvaluateReticle(true, true, true, true, 1.0f).reticle, "hidden while ImGui captures input");
	const auto libre = EvaluateReticle(true, false, false, false, 0.0f);
	Check(libre.reticle && !libre.arc, "square without object");
	Check(!EvaluateReticle(true, false, true, false, 0.0f).arc, "holding without charging has no ring");
	const auto media = EvaluateReticle(true, false, true, true, 0.625f);
	Check(media.arc && media.fraction == 0.5f && !media.full, "half charge");
	const auto completa = EvaluateReticle(true, false, true, true, 5.0f);
	Check(completa.arc && completa.fraction == 1.0f && completa.full, "full charge is clamped");
	Check(!EvaluateReticle(true, false, false, true, 1.0f).arc, "losing the object removes the ring");
	Check(game::interaction::kMaxThrowChargeSeconds == 1.25f && game::interaction::ThrowSpeed(0.0f) == 3.0f &&
		game::interaction::ThrowSpeed(1.0f) == 15.0f, "throw timing and speeds are unchanged");
}

int main()
{
	TestStatusBar();
	TestNetworkStartRequest();
	TestDynamicRows();
	TestReticle();
	if (failures != 0) return 1;
	std::cout << "RuntimeUiTests OK\n";
	return 0;
}
