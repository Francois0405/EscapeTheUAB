#include "src/ui/EscalaUI.h"

#include <cmath>
#include <iostream>

namespace
{
	int errores = 0;

	void comprobarCerca(float actual, float esperado, const char* mensaje)
	{
		if (std::abs(actual - esperado) > 0.0001f)
		{
			std::cerr << "FAIL: " << mensaje << " (actual=" << actual
				<< ", esperado=" << esperado << ")\n";
			++errores;
		}
	}
}

int main()
{
	comprobarCerca(interfaz::CalcularEscalaUI(1280.0f, 720.0f, 1.0f, 1.0f),
		1.0f, "resolucion de referencia a DPI 100% conserva escala 1");
	comprobarCerca(interfaz::CalcularEscalaUI(2560.0f, 1440.0f, 1.0f, 1.0f),
		std::sqrt(1.5f), "resolucion grande limita el factor logico antes de combinarlo");
	comprobarCerca(interfaz::CalcularEscalaUI(1280.0f, 720.0f, 2.0f, 2.0f),
		std::sqrt(2.0f), "DPI 200% se combina geometricamente sin usar el framebuffer nativo");
	comprobarCerca(interfaz::CalcularEscalaUI(640.0f, 360.0f, 1.0f, 1.0f),
		std::sqrt(0.75f), "ventana pequena respeta el limite inferior de resolucion logica");
	comprobarCerca(interfaz::CalcularEscalaUI(3840.0f, 2160.0f, 2.0f, 2.0f),
		std::sqrt(3.0f), "4K con DPI alto alcanza el maximo efectivo de la formula");

	const auto escalada = interfaz::EscalarRectanguloUI({100.0f, 50.0f, 200.0f, 100.0f},
		1.5f, 1280.0f, 720.0f);
	comprobarCerca(escalada.x, 150.0f, "cambio de DPI escala la posicion de ventana");
	comprobarCerca(escalada.y, 75.0f, "cambio de DPI escala la posicion vertical de ventana");
	comprobarCerca(escalada.ancho, 300.0f, "cambio de DPI escala el ancho de ventana");
	comprobarCerca(escalada.alto, 150.0f, "cambio de DPI escala el alto de ventana");

	const auto recortada = interfaz::EscalarRectanguloUI({1000.0f, 500.0f, 400.0f, 300.0f},
		1.5f, 1920.0f, 1080.0f);
	comprobarCerca(recortada.x, 1320.0f, "ventana escalada permanece dentro del ancho logico");
	comprobarCerca(recortada.y, 630.0f, "ventana escalada permanece dentro del alto logico");

	if (errores != 0) return 1;
	std::cout << "PASS: formula de escala UI\n";
	return 0;
}
