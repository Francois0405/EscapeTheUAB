#include "file_dialog_compat.h"
#include "framework.h"
#include "imgui.h"
#include "src/rendering/RuntimeRenderer.h"
#include "src/rendering/shader.h"
#include "src/rendering/UniformCache.h"
#include "src/rendering/visualitzacio.h"
#include "src/editor/EditorEscenas.h"
#include "src/vr/OpenXRManager.h"
#include "src/vr/VistaEstereo.h"
#include "src/vr/VistaVR.h"
#include <algorithm>
#include <cstdio>

namespace
{
	constexpr float vrPlanoCercano = 0.05f;    // metros; el plano lejano de pantalla (50000) arruinaría la precisión de Z
	constexpr float vrPlanoLejano = 500.0f;
}

void RuntimeRenderer::OnSize(GLFWwindow* window, int width, int height)
{
	width_old = width;
	height_old = height;
}

void RuntimeRenderer::OnFramebufferSize(GLFWwindow* window, int width, int height)
{
	w = width;
	h = height;
	if (width > 0 && height > 0)
		glViewport(0, 0, width, height);
}


// OnPaint: Funció de dibuix i visualització en frame buffer del frame
// Vista estereoscópica de PRUEBA (F2): dibuja la escena dos veces, una en cada mitad de la pantalla.
// Reutiliza Vista_Navega del profesor para la vista CENTRAL (limpia los buffers, iluminación, culling,
// profundidad, pan) y solo desplaza esa matriz para cada ojo. No toca la física ni la posición del jugador.
void RuntimeRenderer::dibujarEscenaEstereo(int anchoVentana, int altoVentana)
{
	GLdouble arriba[3] = { 0.0, 0.0, 1.0 };   // Z es "arriba" en este juego
	const glm::dvec3 balanceo = desplazamientoVisualDeCamara();
	CPunt3D ojoVisual = opvN;
	ojoVisual.x += balanceo.x; ojoVisual.y += balanceo.y; ojoVisual.z += balanceo.z;
	GLdouble objetivoVisual[3] = { n[0] + balanceo.x, n[1] + balanceo.y, n[2] + balanceo.z };
	const glm::mat4 vistaCentral = Vista_Navega(shader_programID, ojoVisual, objetivoVisual, arriba, pan, tr_cpv, tr_cpvF, c_fons, col_obj,
		objecte, true, pas, front_faces, oculta, test_vis, back_line, ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
		eixos, grid, hgrid);

	for (const estereo::Ojo ojo : { estereo::Ojo::Izquierdo, estereo::Ojo::Derecho })
	{
		const estereo::Rectangulo mitad = estereo::MitadDePantalla(ojo, anchoVentana);
		// Cada mitad tiene SU relación de aspecto (la mitad de ancho): Projeccio_Perspectiva fija el viewport y la proyección.
		ProjectionMatrix = Projeccio_Perspectiva(shader_programID, mitad.x, 0, mitad.ancho, altoVentana, OPV.R);
		ViewMatrix = estereo::VistaDeOjo(vistaCentral, ojo);

		Iluminacio(
			shader_programID,
			ilumina,
			ifixe,
			ilum2sides,
			llum_ambient,
			llumGL,
			objecte,
			front_faces,
			back_line,
			pas
		);

		glUseProgram(shader_programID);
		glUniformMatrix4fv(glcache::UniformLocation(shader_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);
		Iluminacio(shader_programID, ilumina, ifixe, ilum2sides, llum_ambient, llumGL, objecte, front_faces, back_line, pas);

		configura_Escena();
		dibuixa_Escena();   // habitación, objetos GLB, física de depuración... exactamente lo mismo que en PC
	}
}

// ---------------------------------------------------------------------------------------------
// VR real (OpenXR, F3). Cada ojo se dibuja en SU swapchain con SU pose y SU campo de visión.
// Es el mismo dibujo que en pantalla (configura_Escena + dibuixa_Escena: habitación, OBJ, GLB, puertas,
// colisionadores...); solo cambian las matrices. La cabeza mueve la CÁMARA, nunca la cápsula del jugador:
// el anclaje (pies o ojos del jugador + su orientación FPS) se recalcula cada fotograma.
// ---------------------------------------------------------------------------------------------
// Copia el ojo a la ventana (espejo de escritorio) conservando su proporción. Debe hacerse ANTES de liberar la imagen.
void RuntimeRenderer::copiarOjoAVentana(const vr::OjoVR& ojo)
{
	if (w <= 0 || h <= 0 || ojo.ancho <= 0 || ojo.alto <= 0) return;
	const float escala = std::min(static_cast<float>(w) / ojo.ancho, static_cast<float>(h) / ojo.alto);
	const int anchoDestino = static_cast<int>(ojo.ancho * escala), altoDestino = static_cast<int>(ojo.alto * escala);
	const int x0 = (w - anchoDestino) / 2, y0 = (h - altoDestino) / 2;
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, ojo.fbo);
	glBlitFramebuffer(0, 0, ojo.ancho, ojo.alto, x0, y0, x0 + anchoDestino, y0 + altoDestino, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

// Devuelve true si este fotograma se dibujó en el visor (y en la ventana como espejo). Si devuelve false
// hay que dibujar la escena de escritorio con normalidad (VR sin sesión en marcha, visor sin mostrar, etc.).
bool RuntimeRenderer::dibujarEscenaVR()
{
	const vr::ResultadoFotograma resultado = gestorOpenXR.EmpezarFotograma();
	if (resultado == vr::ResultadoFotograma::NoActivo) return false;

	bool dibujado = false;
	if (resultado == vr::ResultadoFotograma::Renderizar)
	{
		const glm::vec3 pies = piesJugador();
		const glm::vec3 haciaDondeMira(static_cast<float>(n[0] - opvN.x), static_cast<float>(n[1] - opvN.y), static_cast<float>(n[2] - opvN.z));
		const vr::Anclaje anclaje = vr::AnclajeDelJugador(pies, haciaDondeMira,
			gestorOpenXR.Espacio() == vr::EspacioReferencia::Suelo, alturaOjosJugador());

		glDisable(GL_SCISSOR_TEST);
		glm::vec3 sumaOjos(0.0f); int ojosCabeza = 0; vr::PoseXR primerOjo;
		for (int i = 0; i < gestorOpenXR.NumeroDeOjos(); ++i)
		{
			vr::OjoVR ojo;
			if (!gestorOpenXR.AdquirirOjo(i, ojo)) continue;   // el fotograma se enviará vacío; AdquirirOjo no deja nada a medias
			if (ojosCabeza++ == 0) primerOjo = ojo.pose;       // para la red: pose real de la cabeza (media de los ojos)
			sumaOjos += ojo.pose.posicion;

			glViewport(0, 0, ojo.ancho, ojo.alto);
			ProjectionMatrix = vr::ProyeccionDesdeCampo(ojo.fov, vrPlanoCercano, vrPlanoLejano);
			ViewMatrix = vr::VistaDeOjoVR(ojo.pose, anclaje);

			glUseProgram(shader_programID);
			glUniformMatrix4fv(glcache::UniformLocation(shader_programID, "projectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
			glUniformMatrix4fv(glcache::UniformLocation(shader_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);

			Fons(c_fons);   // limpia color y profundidad del framebuffer del ojo (ya enlazado)
			if (test_vis) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
			if (oculta) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
			Iluminacio(shader_programID, ilumina, ifixe, ilum2sides, llum_ambient, llumGL, objecte, front_faces, back_line, pas);

			configura_Escena();
			dibuixa_Escena();

			if (i == 0) copiarOjoAVentana(ojo);   // espejo en el monitor: ojo izquierdo
			gestorOpenXR.LiberarOjo(i);
			dibujado = true;
		}
		if (ojosCabeza > 0)
		{
			ultimaCabezaVR = primerOjo;
			ultimaCabezaVR.posicion = sumaOjos / static_cast<float>(ojosCabeza);
			hayCabezaVR = true;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, w, h);
	}
	gestorOpenXR.TerminarFotograma();
	return dibujado;
}

void RuntimeRenderer::OnPaint(GLFWwindow* window)
{
	if (w <= 0 || h <= 0) return;
	GLdouble vpv[3] = { 0.0, 0.0, 1.0 };

	if (editorEscenas.Activo())
	{
		glDisable(GL_SCISSOR_TEST);
		glViewport(0, 0, w, h);

		// Configuración gráfica del editor.
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glDisable(GL_CULL_FACE);

		// Fondo neutro y limpieza de profundidad.
		glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		const float aspecto = std::max(1.0f, displaySize.x) / std::max(1.0f, displaySize.y);
		editorEscenas.Matrices(aspecto, ViewMatrix, ProjectionMatrix);
		glUseProgram(shader_programID);
		glUniformMatrix4fv(glcache::UniformLocation(shader_programID, "projectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
		glUniformMatrix4fv(glcache::UniformLocation(shader_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);
		glUseProgram(eixos_programID);
		glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "projectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
		glUniformMatrix4fv(glcache::UniformLocation(eixos_programID, "viewMatrix"), 1, GL_FALSE, &ViewMatrix[0][0]);
		configura_Escena();
		dibuixa_Escena();
		return;
	}

// Entorn VGI.ImGui: Menú ImGui condicionat al color de fons
	// Entorn VGI: Cridem a les funcions de l'escena i la projecció segons s'hagi
	// seleccionat una projecció o un altra
	switch (projeccio)
	{
	case AXONOM:
		// Entorn VGI: PROJECCIÓ AXONOMÈTRICA
		// Entorn VGI: Activació del retall de pantalla
		glEnable(GL_SCISSOR_TEST);

		// Entorn VGI: Retall
		glScissor(0, 0, w, h);
		glViewport(0, 0, w, h);

		// Aquí farem les crides per a definir Viewport, Projecció i Càmara: INICI -------------------------

		// Aquí farem les cridesper a definir Viewport, Projecció i Càmara:: FI -------------------------
		// Entorn VGI: Dibuixar Model (escena)
		configura_Escena();     // Aplicar Transformacions Geometriques segons persiana Transformacio i configurar objectes
		dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.

		// Entorn VGI: Transferència del buffer OpenGL a buffer de pantalla
				//glfwSwapBuffers(window);
		break;

	case ORTO:
		// Entorn VGI: PROJECCIÓ ORTOGRÀFICA
		// Entorn VGI: Activació del retall de pantalla
		glEnable(GL_SCISSOR_TEST);

		// Entorn VGI: Retall
		glScissor(0, 0, w, h);
		glViewport(0, 0, w, h);

		// Fons condicionat al color de fons
		if ((c_fons.r < 0.5) || (c_fons.g < 0.5) || (c_fons.b < 0.5))
			FonsB();
		else
			FonsN();

		// Entorn VGI: TO DO -> Aquí farem les quatre crides a ProjeccioOrto i Ortografica per obtenir
		//						les quatre vistes ortogràfiques. De moment n'activem només una de prova
		//						IMPORTANT: DESCOMENTAR LA RESTA QUAN FUNCIONI LA PRIMERA
		// PLANTA (Inferior Esquerra)
				// Definició de Viewport, Projecció i Càmara
		ProjectionMatrix = Projeccio_Orto();
		ViewMatrix = Vista_Ortografica(shader_programID, 0, OPV.R, c_fons, col_obj, objecte, mida, pas, front_faces, oculta,
			test_vis, back_line, ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
			eixos, grid, hgrid);
		// Dibuix de l'Objecte o l'Escena
		configura_Escena();     // Aplicar Transformacions Geometriques segons persiana Transformacio i configurar objectes
		dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.
		/*
		// ISOMÈTRICA (Inferior Dreta)
				// Definició de Viewport, Projecció i Càmara
				ProjectionMatrix = Projeccio_Orto();
				ViewMatrix = Vista_Ortografica(shader_programID, 3, OPV.R, c_fons, col_obj, objecte, mida, pas, front_faces, oculta,
					test_vis, back_line, ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
					eixos, grid, hgrid);
				// Dibuix de l'Objecte o l'Escena
				configura_Escena();     // Aplicar Transformacions Geometriques segons persiana Transformacio i configurar objectes
				dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.

		// ALÇAT (Superior Esquerra)
				// Definició de Viewport, Projecció i Càmara
				ProjectionMatrix = Projeccio_Orto();
				ViewMatrix = Vista_Ortografica(shader_programID, 1, OPV.R, c_fons, col_obj, objecte, mida, pas, front_faces, oculta,
					test_vis, back_line, ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
					eixos, grid, hgrid);
				// Dibuix de l'Objecte o l'Escena
				  configura_Escena();     // Aplicar Transformacions Geom?triques segons persiana Transformacio i configurar objectes
				  dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.

		// PERFIL (Superior Dreta)
				// Definició de Viewport, Projecció i Càmara
				ProjectionMatrix = Projeccio_Orto();
				ViewMatrix = Vista_Ortografica(shader_programID, 2, OPV.R, c_fons, col_obj, objecte, mida, pas, front_faces, oculta,
					test_vis, back_line, ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
					eixos, grid, hgrid);
				// Dibuix de l'Objecte o l'Escena
				configura_Escena();     // Aplicar Transformacions Geom?triques segons persiana Transformacio i configurar objectes
				  // glScalef();			// Escalat d'objectes, per adequar-los a les vistes ortogràfiques (Pràctica 2)
				dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.
		*/
		// Entorn VGI: Transferència del buffer OpenGL a buffer de pantalla
				//glfwSwapBuffers(window);

		break;

	case PERSPECT:
		// Entorn VGI: PROJECCIÓ PERSPECTIVA
				//glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // Set Perspective Calculations To Most Accurate
		glDisable(GL_SCISSOR_TEST);		// Desactivació del retall de pantalla

		// Entorn VGI: Activar shader Visualització Escena
		glUseProgram(shader_programID);

		// Vista estereoscópica de prueba (F2): solo con la cámara de navegación; el resto de cámaras
		// del profesor (esférica, geode) se dibujan siempre como antes.
		if (modoEstereoPrueba && camera == CAM_NAVEGA)
		{
			dibujarEscenaEstereo(w, h);
			break;
		}

		// Entorn VGI: Definició de Viewport, Projecció i Càmara
		ProjectionMatrix = Projeccio_Perspectiva(shader_programID, 0, 0, w, h, OPV.R);

		// Entorn VGI: Definició de la càmera.
		if (camera == CAM_ESFERICA) {
			n[0] = 0;		n[1] = 0;		n[2] = 0;
			ViewMatrix = Vista_Esferica(shader_programID, OPV, Vis_Polar, pan, tr_cpv, tr_cpvF, c_fons, col_obj, objecte, mida, pas,
				front_faces, oculta, test_vis, back_line,
				ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
				eixos, grid, hgrid);
		}
		else if (camera == CAM_NAVEGA) {
			if (Vis_Polar == POLARZ) {
				vpv[0] = 0.0;	vpv[1] = 0.0;	vpv[2] = 1.0;
				}
			else if (Vis_Polar == POLARY) {
				vpv[0] = 0.0;	vpv[1] = 1.0;	vpv[2] = 0.0;
				}
			else if (Vis_Polar == POLARX) {
				vpv[0] = 1.0;	vpv[1] = 0.0;	vpv[2] = 0.0;
				}
			// El balanceo se aplica solo a la imagen: mismo desplazamiento al ojo y al punto mirado (la direccion no cambia).
			const glm::dvec3 balanceo = desplazamientoVisualDeCamara();
			CPunt3D ojoVisual = opvN;
			ojoVisual.x += balanceo.x; ojoVisual.y += balanceo.y; ojoVisual.z += balanceo.z;
			GLdouble objetivoVisual[3] = { n[0] + balanceo.x, n[1] + balanceo.y, n[2] + balanceo.z };
			ViewMatrix = Vista_Navega(shader_programID, ojoVisual, //false,
				objetivoVisual, vpv, pan, tr_cpv, tr_cpvF, c_fons, col_obj, objecte, true, pas,
				front_faces, oculta, test_vis, back_line,
				ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
				eixos, grid, hgrid);
			}
		else if (camera == CAM_GEODE) {
			ViewMatrix = Vista_Geode(shader_programID, OPV_G, Vis_Polar, pan, tr_cpv, tr_cpvF, c_fons, col_obj, objecte, mida, pas,
				front_faces, oculta, test_vis, back_line,
				ilumina, llum_ambient, llumGL, ifixe, ilum2sides,
				eixos, grid, hgrid);
			}

		// Entorn VGI: Dibuix de l'Objecte o l'Escena
		configura_Escena();     // Aplicar Transformacions Geometriques segons persiana Transformacio i configurar objectes.
		dibuixa_Escena();		// Dibuix geometria de l'escena amb comandes GL.

		// Entorn VGI: Transferència del buffer OpenGL a buffer de pantalla
		//glfwSwapBuffers(window);
		break;

	default:
		// Entorn VGI: Càrrega SHADERS
		// Entorn VGI: Càrrega Shader Eixos
		if (!eixos_programID) eixos_programID = shaderEixos.loadFileShaders("./assets/shaders/eixos.vert", "./assets/shaders/eixos.frag");

		// Entorn VGI: Càrrega Shader de Gouraud
		if (!shader_programID) shader_programID = shaderLighting.loadFileShaders("./assets/shaders/gouraud_shdrML.vert", "./assets/shaders/gouraud_shdrML.frag");

		// Entorn VGI: Creació de la llista que dibuixarà els eixos Coordenades Món. Funció on està codi per dibuixar eixos
		if (!eixos_Id) eixos_Id = deixos();						// Funció que defineix els Eixos Coordenades Món com un VAO.

		// Entorn VGI: Crida a la funció Fons Blanc
		FonsB();

		// Entorn VGI: Transferència del buffer OpenGL a buffer de pantalla
		//glfwSwapBuffers(window);
		break;
	}

	//  Actualitzar la barra d'estat de l'aplicació amb els valors R,A,B,PVx,PVy,PVz
	if (statusB) Barra_Estat();
}
