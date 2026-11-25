#include "src/app/AppState.h"
#include "src/app/Shutdown.h"
#include "src/app/adapters/SceneAdapters.h"
#include "src/app/input/CursorCapture.h"
#include "src/rendering/Ps1Framebuffer.h"

void ShutdownApplication(Ps1Framebuffer& ps1)
{
	// Una salida por el boton de Windows tambien devuelve el cursor al sistema.
	fijarCapturaCursor(window, false);
	if (texturaManoInteraccion != 0)
	{
		glDeleteTextures(1, &texturaManoInteraccion);
		texturaManoInteraccion = 0;
	}

// RESET RECURSOS PS1
	ps1.Destroy();
	if (texturaFondoMenu != 0)
	{
		glDeleteTextures(1, &texturaFondoMenu);
		texturaFondoMenu = 0;
	}

// Entorn VGI.ImGui: Cleanup ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	redJuego.Detener();           // cierra la red (si la hay) antes que la ventana
	gestorOpenXR.Desactivar();   // sesión e instancia OpenXR (si las hay) antes de cerrar la ventana
	NFD_QuitCompat();

	for (auto& entrada : modelosHabitacion)
		entrada.second->netejaTextures_OBJ();

	modelosHabitacion.clear();
	objetosEscena.clear();
	liberarObjetosAnimados();
	mundoFisico.EliminarJugador();
	escenaFisica.reset();                       // quita las colisiones de la habitacion antes de cerrar
	fisica::LiberarDibujoColisionadores();
	netjuego::LiberarDibujoAvatares();

	if (phoneOBJ != nullptr)
	{
		phoneOBJ->netejaVAOList_OBJ();
		phoneOBJ->netejaTextures_OBJ();
		delete phoneOBJ;
		phoneOBJ = nullptr;
	}

	if (Get_VAOId(CUBE_SKYBOX) != 0) deleteVAOList(CUBE_SKYBOX);
	netejaVAOList();
	if (eixos_Id != 0) glDeleteVertexArrays(1, &eixos_Id);
	if (rayoVao != 0) glDeleteVertexArrays(1, &rayoVao);
	if (rayoVbo != 0) glDeleteBuffers(1, &rayoVbo);
	for (int textureIndex = 0; textureIndex < NUM_MAX_TEXTURES; ++textureIndex)
	{
		if (texturesID[textureIndex] != 0)
		{
			glDeleteTextures(1, &texturesID[textureIndex]);
			texturesID[textureIndex] = 0;
		}
	}
	if (cubemapTexture != 0)
	{
		glDeleteTextures(1, &cubemapTexture);
		cubemapTexture = 0;
	}

	// Delete GPU programs while the OpenGL context that created them is current.
	glcache::InvalidateAll(); oclusoresPhongInvalidos = true;
	if (consultasGpu[0] != 0) glDeleteQueries(3, consultasGpu);
	if (shaderLighting.getProgramID() != -1) shaderLighting.DeleteProgram();
	if (shaderSkyBox.getProgramID() != -1) shaderSkyBox.DeleteProgram();
	if (shaderEixos.getProgramID() != -1) shaderEixos.DeleteProgram();
	if (shader_SkyBoxC.getProgramID() != -1) shader_SkyBoxC.DeleteProgram();

	glfwDestroyWindow(window);

// Terminating GLFW: Destroy the windows, monitors and cursor objects
	glfwTerminate();
}
