//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (Escola Enginyeria - UAB)
//******** Entorn bàsic VS2026 MONOFINESTRA amb OpenGL 4.6, interfície GLFW 3.4, ImGui i llibreries GLM
//******** Ferran Poveda, Marc Vivet, Carme Julià, Débora Gil, Enric Martí (Setembre 2026)
// main.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//    Versió 0.5:	- Interficie ImGui
//					- Per a dialeg de cerca de fitxers, s'utilitza la llibreria NativeFileDialog
//

// Entorn VGI.ImGui: Includes llibreria ImGui

#include "src/app/AppIncludes.h"
#include "src/app/state/GameplayState.h"
#include "src/app/Application.h"
#include "src/app/MainLoop.h"
#include "src/app/Shutdown.h"
#include "src/app/WorkingDirectory.h"
#include "src/rendering/Ps1Framebuffer.h"

int main(int argc, char** argv)
{
	SetWorkingDirectoryToAssetRoot(argc > 0 ? argv[0] : nullptr);
	audioManager.init();
	ParseNetworkArguments(argc, argv);

	if (!CreateMainWindow()) return -1;
	InitializeGraphicsResources();
	RegisterWindowCallbacks();

	// Entorn VGI; Timer: Lectura temps
	const float previous = glfwGetTime();

	InitializeImGui();
	if (!StartGame()) return EXIT_FAILURE;

	Ps1Framebuffer ps1;
	ps1.Create();
	LoadMenuBackground();

	RunMainLoop(ps1, previous);
	ShutdownApplication(ps1);
	return 0;
}
