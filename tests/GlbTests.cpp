// GlbTests.cpp
// -----------------------------------------------------------------------------
// Pruebas del sistema de animaciones GLB (no necesitan ventana ni OpenGL).
//
//   1. Matemática de keyframes: interpolación, escalón, extremos, reproductor.
//   2. La puerta real (assets/animations/Puerta.glb): jerarquía, pivote, animación.
//   3. Un GLB inventado dentro de la prueba (un "cajón") con una jerarquía DISTINTA
//      a la de la puerta, para comprobar que el cargador no depende de ella.
// -----------------------------------------------------------------------------
#include "src/rendering/GlbModel.h"
#include "src/animation/GlbAnimatedObjects.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace glb;

namespace
{
	int fallos = 0;

	void comprobar(bool condicion, const char* caso)
	{
		if (!condicion)
		{
			std::cerr << "FALLO: " << caso << '\n';
			++fallos;
		}
	}

	bool casi(float a, float b, float tolerancia = 1.0e-3f) { return std::fabs(a - b) <= tolerancia; }

	bool casiVec(const glm::vec3& a, const glm::vec3& b, float tolerancia = 1.0e-3f)
	{
		return glm::length(a - b) <= tolerancia;
	}

	// Posición en el espacio del glTF (Y arriba) de un punto local de un nodo.
	glm::vec3 PuntoEnMundo(const GlbModel& modelo, const GlbPose& pose, int nodo, const glm::vec3& puntoLocal)
	{
		return glm::vec3(NodeWorldMatrix(modelo, pose, nodo) * glm::vec4(puntoLocal, 1.0f));
	}

	// Ángulo de giro (grados) de un cuaternión.
	float AnguloGrados(const glm::quat& q)
	{
		return glm::degrees(2.0f * std::acos(std::min(1.0f, std::fabs(q.w))));
	}

	// ----------------------------------------------------------------------
	// 1. Matemática de keyframes (canales escritos a mano)
	// ----------------------------------------------------------------------
	AnimationChannel CanalPosicion(InterpolationMode interpolacion)
	{
		AnimationChannel canal;
		canal.propiedad = AnimatedProperty::Posicion;
		canal.interpolacion = interpolacion;
		canal.tiempos = { 0.0f, 2.0f, 4.0f };
		canal.valores = { glm::vec4(0, 0, 0, 0), glm::vec4(10, 0, 0, 0), glm::vec4(10, 20, 0, 0) };
		return canal;
	}

	void ProbarInterpolacion()
	{
		const AnimationChannel lineal = CanalPosicion(InterpolationMode::Lineal);
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, 0.0f)), glm::vec3(0, 0, 0)), "t=0 devuelve el primer keyframe");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, 1.0f)), glm::vec3(5, 0, 0)), "mitad del primer tramo = mix al 50%");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, 2.0f)), glm::vec3(10, 0, 0)), "t justo en el keyframe intermedio");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, 3.0f)), glm::vec3(10, 10, 0)), "mitad del segundo tramo");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, -5.0f)), glm::vec3(0, 0, 0)), "antes del inicio: se queda en el primero");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(lineal, 99.0f)), glm::vec3(10, 20, 0)), "despues del final: se queda en el ultimo");

		const AnimationChannel escalon = CanalPosicion(InterpolationMode::Escalon);
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(escalon, 1.9f)), glm::vec3(0, 0, 0)), "escalon mantiene el valor anterior");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(escalon, 2.0f)), glm::vec3(10, 0, 0)), "escalon salta al llegar al keyframe");

		// Rotación: de 0 a 90 grados alrededor de Y. A mitad de camino deben ser 45 grados.
		AnimationChannel giro;
		giro.propiedad = AnimatedProperty::Rotacion;
		giro.tiempos = { 0.0f, 2.0f };
		const glm::quat q90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
		giro.valores = { glm::vec4(0, 0, 0, 1), glm::vec4(q90.x, q90.y, q90.z, q90.w) };
		const glm::quat mitad = QuaternionFromVec4(ChannelValueAtTime(giro, 1.0f));
		comprobar(casi(AnguloGrados(mitad), 45.0f, 0.05f), "slerp: la mitad de 90 grados son 45");
		comprobar(casi(glm::length(mitad), 1.0f), "slerp conserva un cuaternion unitario");

		// Un canal con un solo keyframe no debe fallar.
		AnimationChannel unico;
		unico.tiempos = { 1.0f };
		unico.valores = { glm::vec4(7, 8, 9, 0) };
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(unico, 0.0f)), glm::vec3(7, 8, 9)), "un solo keyframe");
		comprobar(casiVec(glm::vec3(ChannelValueAtTime(unico, 5.0f)), glm::vec3(7, 8, 9)), "un solo keyframe, despues");
	}

	void ProbarReproductor()
	{
		AnimationPlayer reproductor;
		reproductor.Start(2.0f);
		comprobar(reproductor.reproduciendo && reproductor.tiempo == 0.0f, "Iniciar pone el tiempo a 0");
		reproductor.Update(1.5f);
		comprobar(reproductor.reproduciendo && casi(reproductor.tiempo, 1.5f), "avanza con el delta");
		reproductor.Update(1.0f);
		comprobar(!reproductor.reproduciendo, "termina al pasar la duracion");
		comprobar(reproductor.tiempo == 2.0f, "al terminar se queda justo en la duracion");
		reproductor.Update(5.0f);
		comprobar(reproductor.tiempo == 2.0f, "ya terminado, no sigue avanzando");
	}

	void ProbarReproductorHaciaAtras()
	{
		AnimationPlayer reproductor;
		reproductor.Start(2.0f);
		reproductor.Update(0.0f);
		comprobar(reproductor.reproduciendo && reproductor.tiempo == 0.0f, "Iniciar + delta 0 no termina la reproduccion");

		reproductor.Update(1.0f);                       // t = 1, hacia delante
		reproductor.PlayAnimation(-1.0f);                      // invertir a mitad de camino
		comprobar(reproductor.reproduciendo && casi(reproductor.tiempo, 1.0f), "invertir no cambia el instante actual (sin saltos)");
		reproductor.Update(0.25f);
		comprobar(casi(reproductor.tiempo, 0.75f), "hacia atras el tiempo disminuye");
		reproductor.Update(5.0f);
		comprobar(!reproductor.reproduciendo && reproductor.tiempo == 0.0f, "hacia atras se detiene en 0 y no baja de ahi");
		reproductor.PlayAnimation(+1.0f);
		reproductor.Update(0.5f);
		comprobar(reproductor.reproduciendo && casi(reproductor.tiempo, 0.5f), "volver a ir hacia delante desde 0");
	}

	void ProbarConversionDeCoordenadas()
	{
		// glTF (X, Y, Z) -> juego (X, -Z, Y)
		const glm::vec3 convertido = glm::vec3(YUpToZUpMatrix() * glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
		comprobar(casiVec(convertido, glm::vec3(1.0f, -3.0f, 2.0f)), "Y-arriba -> Z-arriba: (1,2,3) pasa a (1,-3,2)");
		const glm::vec3 arriba = glm::vec3(YUpToZUpMatrix() * glm::vec4(0, 1, 0, 0));
		comprobar(casiVec(arriba, glm::vec3(0, 0, 1)), "el eje Y de glTF (arriba) pasa a ser Z (arriba del juego)");
	}

	// ----------------------------------------------------------------------
	// 2. La puerta real
	// ----------------------------------------------------------------------
	void ProbarPuertaReal(const fs::path& ruta)
	{
		GlbModel modelo;
		std::string error;
		comprobar(LoadGlbModel(ruta, modelo, error), "Puerta.glb carga");
		if (modelo.nodos.empty()) { std::cerr << "  " << error << '\n'; return; }
		GlbPose pose = CreateRestPose(modelo);   // la pose de esta "puerta"

		// --- Jerarquía ---
		const int grupo = FindNodeByName(modelo, "Door_Group");
		const int marco = FindNodeByName(modelo, "DoorFrame");
		const int puerta = FindNodeByName(modelo, "Door");
		const int pomoDelante = FindNodeByName(modelo, "Handle_Front");
		const int pomoDetras = FindNodeByName(modelo, "Handle_Back");
		comprobar(grupo >= 0 && marco >= 0 && puerta >= 0 && pomoDelante >= 0 && pomoDetras >= 0, "existen los 5 nodos");
		comprobar(FindNodeByName(modelo, "CTRL_Hole") == -1, "los objetos CTRL_ ocultos no se exportan");
		if (grupo < 0 || marco < 0 || puerta < 0 || pomoDelante < 0 || pomoDetras < 0) return;
		comprobar(modelo.nodos[grupo].padre == -1, "Door_Group es raiz");
		comprobar(modelo.nodos[marco].padre == grupo, "DoorFrame es hijo de Door_Group");
		comprobar(modelo.nodos[puerta].padre == marco, "Door es hija de DoorFrame");
		comprobar(modelo.nodos[pomoDelante].padre == puerta && modelo.nodos[pomoDetras].padre == puerta, "los pomos son hijos de Door");

		// --- Mallas ---
		bool mallasValidas = !modelo.mallas.empty();
		for (const GlbMesh& malla : modelo.mallas)
			for (const GlbPrimitive& primitiva : malla.primitivas)
			{
				mallasValidas = mallasValidas && !primitiva.posiciones.empty() && primitiva.indices.size() % 3 == 0 &&
					primitiva.normales.size() == primitiva.posiciones.size();
				for (unsigned int indice : primitiva.indices)
					mallasValidas = mallasValidas && indice < primitiva.posiciones.size();
			}
		comprobar(mallasValidas, "todas las primitivas tienen triangulos, normales e indices validos");

		// --- Pivote: la malla de la puerta llega hasta la bisagra (origen del nodo) ---
		const GlbMesh& mallaPuerta = modelo.mallas[modelo.nodos[puerta].indiceMalla];
		float xMin = 1e9f, xMax = -1e9f;
		for (const GlbPrimitive& p : mallaPuerta.primitivas)
			for (const glm::vec3& v : p.posiciones) { xMin = std::min(xMin, v.x); xMax = std::max(xMax, v.x); }
		comprobar(casi(xMax, 0.0f, 0.01f) && casi(xMin, -0.836f, 0.01f), "el origen de Door esta en el borde de la bisagra, no en el centro");
		// Blender (0.418, -0.022, 1.05) debe llegar como glTF (0.418, 1.05, 0.022): Y arriba.
		comprobar(casiVec(modelo.nodos[puerta].reposo.posicion, glm::vec3(0.418f, 1.05f, 0.0221f), 0.01f), "la bisagra llega en coordenadas Y-arriba");

		// --- Animación ---
		comprobar(modelo.animaciones.size() == 1, "hay una animacion");
		if (modelo.animaciones.empty()) return;
		const GlbAnimation& animation = modelo.animaciones[0];
		comprobar(animation.nombre.rfind("Door_acci", 0) == 0, "la animacion conserva el nombre de Blender (Action Door_accion)");
		comprobar(casi(animation.duracion, 7.0417f, 0.01f), "duracion = 170 fotogramas a 24 fps (7.04 s)");
		bool empiezaEnCero = !animation.canales.empty();
		for (const AnimationChannel& canal : animation.canales)
			empiezaEnCero = empiezaEnCero && canal.tiempos.front() == 0.0f && canal.indiceNodo == puerta;
		comprobar(empiezaEnCero, "todos los canales empiezan en t=0 y animan el nodo Door");

		// Interpolación contra el archivo: a mitad entre dos keyframes, el valor es el slerp de ambos.
		for (const AnimationChannel& canal : animation.canales)
		{
			if (canal.propiedad != AnimatedProperty::Rotacion || canal.tiempos.size() < 20) continue;
			const size_t k = 10;
			const float tMedio = 0.5f * (canal.tiempos[k] + canal.tiempos[k + 1]);
			const glm::quat esperado = glm::slerp(QuaternionFromVec4(canal.valores[k]), QuaternionFromVec4(canal.valores[k + 1]), 0.5f);
			const glm::quat obtenido = QuaternionFromVec4(ChannelValueAtTime(canal, tMedio));
			comprobar(casi(std::fabs(glm::dot(esperado, obtenido)), 1.0f, 1.0e-5f), "entre dos keyframes del archivo el valor es el slerp");
		}

		// Pose inicial y final.
		const glm::vec3 bisagraInicial = PuntoEnMundo(modelo, pose, puerta, glm::vec3(0.0f));
		const glm::vec3 bordeLibreInicial = PuntoEnMundo(modelo, pose, puerta, glm::vec3(xMin, 0.0f, 0.0f));
		const glm::vec3 pomoInicial = PuntoEnMundo(modelo, pose, pomoDelante, glm::vec3(0.0f));

		ApplyAnimation(pose, animation, 0.0f);
		comprobar(casi(AnguloGrados(pose.nodos[puerta].rotacion), 0.0f, 0.5f), "t=0: la puerta esta cerrada (0 grados)");

		ApplyAnimation(pose, animation, animation.duracion);
		const float anguloFinal = AnguloGrados(pose.nodos[puerta].rotacion);
		comprobar(anguloFinal > 80.0f && anguloFinal < 90.0f, "al final la puerta esta abierta unos 85 grados");
		const glm::quat rotacionFinal = pose.nodos[puerta].rotacion;
		comprobar(std::fabs(rotacionFinal.y) > 0.5f && std::fabs(rotacionFinal.x) < 0.01f && std::fabs(rotacionFinal.z) < 0.01f,
			"el giro es alrededor del eje Y de glTF (el Z de Blender)");

		// Al terminar se queda abierta: pasarse de tiempo no cambia nada.
		ApplyAnimation(pose, animation, animation.duracion + 100.0f);
		comprobar(casi(AnguloGrados(pose.nodos[puerta].rotacion), anguloFinal, 0.01f), "despues del final la puerta sigue abierta");

		// --- Pivote durante la animación ---
		const glm::vec3 bisagraFinal = PuntoEnMundo(modelo, pose, puerta, glm::vec3(0.0f));
		const glm::vec3 bordeLibreFinal = PuntoEnMundo(modelo, pose, puerta, glm::vec3(xMin, 0.0f, 0.0f));
		comprobar(casiVec(bisagraInicial, bisagraFinal), "la bisagra no se mueve al abrir la puerta");
		comprobar(glm::length(bordeLibreFinal - bordeLibreInicial) > 0.5f, "el borde libre si se mueve");
		comprobar(casi(glm::length(bordeLibreInicial - bisagraInicial), glm::length(bordeLibreFinal - bisagraFinal), 0.005f),
			"el borde libre sigue a la misma distancia de la bisagra (gira, no se deforma)");

		// --- Los hijos siguen al padre ---
		const glm::vec3 pomoFinal = PuntoEnMundo(modelo, pose, pomoDelante, glm::vec3(0.0f));
		comprobar(glm::length(pomoFinal - pomoInicial) > 0.2f, "el pomo (hijo) se mueve cuando gira la puerta (padre)");
		comprobar(casi(glm::length(pomoInicial - bisagraInicial), glm::length(pomoFinal - bisagraFinal), 0.005f),
			"el pomo mantiene su distancia a la bisagra");
		// El marco (padre) no se anima: no se mueve.
		comprobar(casiVec(PuntoEnMundo(modelo, pose, marco, glm::vec3(0.0f)), glm::vec3(0.0f)), "el marco (no animado) no se mueve");

		// --- Volver al reposo ---
		RestoreRestPose(modelo, pose);
		comprobar(casiVec(PuntoEnMundo(modelo, pose, puerta, glm::vec3(xMin, 0.0f, 0.0f)), bordeLibreInicial), "RestaurarPoseDeReposo devuelve la pose del archivo");
	}

	void ProbarPuertaLaboratorio(const fs::path& ruta)
	{
		GlbModel modelo;
		std::string error;
		comprobar(LoadGlbModel(ruta, modelo, error), "Puerta_Lab.glb carga");
		if (modelo.animaciones.empty()) return;
		comprobar(casi(modelo.animaciones[0].duracion, 3.5208f, 0.02f), "la animacion suavizada se reduce de 7.04 a 3.52 segundos");
		const int marco = FindNodeByName(modelo, "DoorFrame");
		const int hoja = FindNodeByName(modelo, "Door");
		if (marco < 0 || hoja < 0) return;
		const auto& colorMarco = modelo.mallas[modelo.nodos[marco].indiceMalla].primitivas[0].colorBase;
		const auto& colorHoja = modelo.mallas[modelo.nodos[hoja].indiceMalla].primitivas[0].colorBase;
		comprobar(colorMarco.r < colorHoja.r && colorHoja.r < 0.3f,
			"marco oscuro y hoja de metal grafito reciben factores de material distintos");
	}

	// ----------------------------------------------------------------------
	// 3. GLB inventado: un "cajón" con otra jerarquía
	// ----------------------------------------------------------------------
	template <typename T>
	void AgregarBytes(std::vector<unsigned char>& destino, const T& valor)
	{
		const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&valor);
		destino.insert(destino.end(), bytes, bytes + sizeof(T));
	}

	void RellenarHastaMultiploDe4(std::vector<unsigned char>& datos, unsigned char relleno)
	{
		while (datos.size() % 4 != 0) datos.push_back(relleno);
	}

	// Escribe un .glb mínimo. 'interpolacion' es "LINEAR", "STEP" o "CUBICSPLINE".
	//
	// Nodos (a propósito el HIJO va ANTES que el padre, y el padre usa 'matrix'):
	//   nodo 0 "Cajon"  : hijo; lleva la malla (un triángulo) y es el que se anima
	//   nodo 1 "Mueble" : padre; matriz = escala 2 y desplazamiento (10, 0, 0)
	// Animación: el cajón sube de (0,0,0) a (0,0,1) entre t=1 y t=3 (Blender empieza en 1, no en 0).
	void EscribirGlbCajon(const fs::path& ruta, const std::string& interpolacion)
	{
		std::vector<unsigned char> binario;
		for (float f : { 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f }) AgregarBytes(binario, f);   // 0..35: 3 posiciones
		for (unsigned short i : { 0, 1, 2 }) AgregarBytes(binario, i);                              // 36..41: 3 índices
		RellenarHastaMultiploDe4(binario, 0);                                                            // 42..43
		for (float t : { 1.0f, 3.0f }) AgregarBytes(binario, t);                                    // 44..51: tiempos
		for (float f : { 0.f, 0.f, 0.f,  0.f, 0.f, 1.f,  0.f, 0.f, 0.f,  0.f, 0.f, 0.f,  0.f, 0.f, 1.f,  0.f, 0.f, 0.f })
			AgregarBytes(binario, f);                                                               // 52..123: valores

		const bool cubica = (interpolacion == "CUBICSPLINE");
		const std::string json =
			R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[1]}],)"
			R"("nodes":[{"name":"Cajon","mesh":0},{"name":"Mueble","children":[0],"matrix":[2,0,0,0,0,2,0,0,0,0,2,0,10,0,0,1]}],)"
			R"("meshes":[{"name":"Triangulo","primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
			R"("animations":[{"name":"Subir","samplers":[{"input":2,"output":3,"interpolation":")" + interpolacion + R"("}],)"
			R"("channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],)"
			R"("buffers":[{"byteLength":124}],)"
			R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},)"
			R"({"buffer":0,"byteOffset":44,"byteLength":8},{"buffer":0,"byteOffset":52,"byteLength":72}],)"
			R"("accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},)"
			R"({"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},)"
			R"({"bufferView":2,"componentType":5126,"count":2,"type":"SCALAR","min":[1],"max":[3]},)"
			R"({"bufferView":3,"componentType":5126,"count":)" + std::string(cubica ? "6" : "2") + R"(,"type":"VEC3"}]})";

		std::vector<unsigned char> chunkJson(json.begin(), json.end());
		RellenarHastaMultiploDe4(chunkJson, ' ');
		RellenarHastaMultiploDe4(binario, 0);

		std::vector<unsigned char> archivo;
		AgregarBytes(archivo, std::uint32_t(0x46546C67));   // "glTF"
		AgregarBytes(archivo, std::uint32_t(2));
		AgregarBytes(archivo, std::uint32_t(12 + 8 + chunkJson.size() + 8 + binario.size()));
		AgregarBytes(archivo, std::uint32_t(chunkJson.size()));
		AgregarBytes(archivo, std::uint32_t(0x4E4F534A));   // "JSON"
		archivo.insert(archivo.end(), chunkJson.begin(), chunkJson.end());
		AgregarBytes(archivo, std::uint32_t(binario.size()));
		AgregarBytes(archivo, std::uint32_t(0x004E4942));   // "BIN\0"
		archivo.insert(archivo.end(), binario.begin(), binario.end());

		std::ofstream salida(ruta, std::ios::binary);
		salida.write(reinterpret_cast<const char*>(archivo.data()), static_cast<std::streamsize>(archivo.size()));
	}

	void ProbarGlbInventado(const fs::path& carpeta)
	{
		const fs::path ruta = carpeta / "cajon.glb";
		EscribirGlbCajon(ruta, "LINEAR");
		GlbModel modelo;
		std::string error;
		comprobar(LoadGlbModel(ruta, modelo, error), "el GLB inventado carga");
		if (modelo.nodos.size() != 2) { comprobar(false, "el GLB inventado tiene 2 nodos"); std::cerr << "  " << error << '\n'; return; }
		GlbPose pose = CreateRestPose(modelo);

		// Jerarquía aunque el hijo esté escrito antes que el padre.
		const int cajon = FindNodeByName(modelo, "Cajon");
		const int mueble = FindNodeByName(modelo, "Mueble");
		comprobar(cajon == 0 && mueble == 1, "los nodos conservan su orden del archivo");
		comprobar(modelo.nodos[cajon].padre == mueble && modelo.nodos[mueble].padre == -1, "padre/hijo correctos con el hijo escrito primero");

		// El nodo del padre venía como 'matrix': se descompone en posición/escala.
		comprobar(casiVec(modelo.nodos[mueble].reposo.posicion, glm::vec3(10, 0, 0)), "matrix: posicion (10,0,0)");
		comprobar(casiVec(modelo.nodos[mueble].reposo.escala, glm::vec3(2, 2, 2)), "matrix: escala 2");
		comprobar(casiVec(PuntoEnMundo(modelo, pose, cajon, glm::vec3(1, 0, 0)), glm::vec3(12, 0, 0)), "mundo(hijo) = mundo(padre) * local(hijo)");

		// Sin normales en el archivo: se calculan. El triángulo está en el plano XY, mirando a +Z.
		const GlbPrimitive& triangulo = modelo.mallas[0].primitivas[0];
		comprobar(triangulo.normales.size() == 3 && casiVec(triangulo.normales[0], glm::vec3(0, 0, 1)), "normales calculadas cuando faltan");
		comprobar(triangulo.colorBase == glm::vec4(1.0f), "sin material: color base blanco");

		// Animación: tiempos 1..3 se normalizan a 0..2.
		comprobar(modelo.animaciones.size() == 1 && modelo.animaciones[0].nombre == "Subir", "animacion 'Subir'");
		const GlbAnimation& subir = modelo.animaciones[0];
		comprobar(casi(subir.duracion, 2.0f) && subir.canales[0].tiempos.front() == 0.0f, "los tiempos se normalizan para empezar en 0");

		// A mitad (t=1 s) el cajón está a 0.5 de altura LOCAL; el padre escala x2 => 1.0 en el mundo.
		ApplyAnimation(pose, subir, 1.0f);
		comprobar(casiVec(pose.nodos[cajon].posicion, glm::vec3(0, 0, 0.5f)), "interpolacion lineal de la posicion local");
		comprobar(casiVec(PuntoEnMundo(modelo, pose, cajon, glm::vec3(0.0f)), glm::vec3(10, 0, 1.0f)), "la escala del padre afecta al movimiento del hijo");
		ApplyAnimation(pose, subir, 99.0f);
		comprobar(casiVec(PuntoEnMundo(modelo, pose, cajon, glm::vec3(0.0f)), glm::vec3(10, 0, 2.0f)), "al final el cajon se queda arriba");
		RestoreRestPose(modelo, pose);
		comprobar(casiVec(PuntoEnMundo(modelo, pose, cajon, glm::vec3(0.0f)), glm::vec3(10, 0, 0)), "reposo del cajon");

		// STEP: no interpola.
		EscribirGlbCajon(ruta, "STEP");
		GlbModel escalon;
		comprobar(LoadGlbModel(ruta, escalon, error) && !escalon.animaciones.empty(), "GLB con STEP carga");
		if (!escalon.animaciones.empty())
		{
			GlbPose poseEscalon = CreateRestPose(escalon);
			ApplyAnimation(poseEscalon, escalon.animaciones[0], 1.9f);
			comprobar(casiVec(poseEscalon.nodos[0].posicion, glm::vec3(0, 0, 0)), "STEP mantiene el valor anterior");
		}

		// CUBICSPLINE: se rechaza con un mensaje que explica cómo arreglarlo.
		EscribirGlbCajon(ruta, "CUBICSPLINE");
		GlbModel cubica;
		const bool cargo = LoadGlbModel(ruta, cubica, error);
		comprobar(!cargo, "CUBICSPLINE se rechaza");
		comprobar(error.find("CUBICSPLINE") != std::string::npos && error.find("Sample") != std::string::npos, "el error explica que hay que activar 'Always Sample Animations'");
		comprobar(cubica.nodos.empty(), "si falla, el modelo queda vacio");
	}

	// ----------------------------------------------------------------------
	// 4. Lista de objetos animados: varias puertas con el mismo modelo
	// ----------------------------------------------------------------------
	// Ángulo (grados) al que está girada la hoja de una puerta en su pose actual.
	float AnguloDeLaHoja(const AnimatedObject& objeto)
	{
		const int hoja = FindNodeByName(*objeto.modelo, "Door");
		return AnguloGrados(objeto.pose.nodos[hoja].rotacion);
	}

	void ProbarObjetosAnimados(const fs::path& rutaPuerta, const fs::path& carpeta)
	{
		AnimatedScene escena;
		std::string error;
		const std::string ruta = rutaPuerta.string();
		comprobar(AddAnimatedObject(escena, "puerta_1", ruta, glm::vec3(6, -2, -1.7f), -90.0f, error), "anadir puerta_1");
		comprobar(AddAnimatedObject(escena, "puerta_2", ruta, glm::vec3(6, 3, -1.7f), 0.0f, error), "anadir puerta_2 (mismo .glb)");
		if (escena.objetos.size() != 2) return;

		// Un solo modelo para las dos instancias.
		comprobar(escena.modelos.size() == 1, "dos instancias del mismo .glb cargan el modelo una sola vez");
		comprobar(escena.objetos[0].modelo == escena.objetos[1].modelo, "las dos instancias comparten el mismo modelo");
		comprobar(escena.objetos[0].modelo == &escena.modelos.at(ruta), "el puntero al modelo apunta al de la lista");

		// Ids únicos y errores sin efectos secundarios.
		comprobar(!AddAnimatedObject(escena, "puerta_1", ruta, glm::vec3(0.0f), 0.0f, error) && error.find("puerta_1") != std::string::npos, "id repetido: error");
		comprobar(!AddAnimatedObject(escena, "otra", (carpeta / "no_existe.glb").string(), glm::vec3(0.0f), 0.0f, error), "modelo inexistente: error");
		comprobar(escena.objetos.size() == 2 && escena.modelos.size() == 1, "los errores no cambian la lista");
		comprobar(FindAnimatedObject(escena, "puerta_2") == &escena.objetos[1] && FindAnimatedObject(escena, "nada") == nullptr, "BuscarObjeto");

		// Punteros estables: cargar otro modelo no invalida el de las puertas.
		const GlbModel* antes = escena.objetos[0].modelo;
		EscribirGlbCajon(carpeta / "otro.glb", "LINEAR");
		comprobar(AddAnimatedObject(escena, "cajon_1", (carpeta / "otro.glb").string(), glm::vec3(0.0f), 0.0f, error), "anadir un modelo distinto");
		comprobar(escena.modelos.size() == 2 && antes == escena.objetos[0].modelo && antes == &escena.modelos.at(ruta), "cargar otro modelo no mueve los anteriores");

		// Colocación: posición y giro de cada instancia.
		const glm::mat4 base = ObjectBaseMatrix(escena.objetos[0]);
		comprobar(casiVec(glm::vec3(base * glm::vec4(0, 0, 0, 1)), glm::vec3(6, -2, -1.7f)), "el origen del modelo va a la posicion de la instancia");
		comprobar(casiVec(glm::vec3(base * glm::vec4(1, 0, 0, 0)), glm::vec3(0, -1, 0)), "giro -90 grados: el eje X pasa a -Y");

		// Independencia: se abre solo la puerta_1.
		AnimatedObject& p1 = *FindAnimatedObject(escena, "puerta_1");
		AnimatedObject& p2 = *FindAnimatedObject(escena, "puerta_2");
		const float duracion = p1.modelo->animaciones[0].duracion;
		comprobar(casi(AnguloDeLaHoja(p1), 0.0f, 0.5f) && casi(AnguloDeLaHoja(p2), 0.0f, 0.5f), "al cargar, las dos puertas estan cerradas");
		BeginAnimation(p1);
		UpdateAnimatedObjects(escena, 2.0f);
		comprobar(casi(p1.reproductor.tiempo, 2.0f) && p2.reproductor.tiempo == 0.0f, "cada instancia tiene su propio reloj");
		comprobar(AnguloDeLaHoja(p1) > 30.0f, "la puerta_1 se abre");
		comprobar(casi(AnguloDeLaHoja(p2), 0.0f, 0.5f), "la puerta_2 sigue cerrada: pose independiente");
		comprobar(&p1.pose != &p2.pose && p1.pose.nodos.size() == p2.pose.nodos.size(), "cada instancia tiene su propia pose");

		// Pausa (delta 0): nada avanza.
		const float anguloPausa = AnguloDeLaHoja(p1);
		UpdateAnimatedObjects(escena, 0.0f);
		comprobar(casi(AnguloDeLaHoja(p1), anguloPausa, 1.0e-4f), "con delta 0 la animacion esta pausada");

		// Abrir la segunda mientras la primera sigue: tiempos distintos.
		// (La animacion abre casi del todo en el primer segundo y luego rebota un poco, por eso
		// se compara con la segunda puerta recien iniciada.)
		BeginAnimation(p2);
		UpdateAnimatedObjects(escena, 0.1f);
		comprobar(casi(p1.reproductor.tiempo, 2.1f) && casi(p2.reproductor.tiempo, 0.1f), "relojes independientes con tiempos distintos");
		comprobar(AnguloDeLaHoja(p1) > AnguloDeLaHoja(p2) + 20.0f && AnguloDeLaHoja(p2) < 30.0f,
			"la puerta_1 ha avanzado claramente mas que la puerta_2, que acaba de empezar");

		// Final: p1 termina y se queda abierta; p2 sigue su camino.
		UpdateAnimatedObjects(escena, duracion);
		comprobar(!p1.reproductor.reproduciendo && !p2.reproductor.reproduciendo, "ambas terminan");
		comprobar(AnguloDeLaHoja(p1) > 80.0f && casi(AnguloDeLaHoja(p1), AnguloDeLaHoja(p2), 0.01f), "al terminar las dos quedan abiertas igual");

		// Reiniciar una devuelve SOLO esa a la pose inicial.
		BeginAnimation(p1);
		UpdateAnimatedObjects(escena, 0.0f);
		comprobar(casi(AnguloDeLaHoja(p1), 0.0f, 0.5f) && AnguloDeLaHoja(p2) > 80.0f, "reiniciar una puerta no afecta a la otra");

		// Una instancia con una animacion que no existe no debe fallar.
		p2.indiceAnimacion = 99;
		BeginAnimation(p2);
		UpdateAnimatedObjects(escena, 1.0f);
		comprobar(true, "indice de animacion invalido no falla");

		ClearAnimatedScene(escena);
		comprobar(escena.objetos.empty() && escena.modelos.empty(), "VaciarEscenaAnimada");
	}

	// ----------------------------------------------------------------------
	// 5. Estados de la puerta y tecla E
	// ----------------------------------------------------------------------
	void ProbarEstadosDePuerta(const fs::path& rutaPuerta)
	{
		AnimatedScene escena;
		std::string error;
		const std::string ruta = rutaPuerta.string();
		comprobar(AddAnimatedObject(escena, "puerta_1", ruta, glm::vec3(6, -2, -1.7f), -90.0f, error), "anadir puerta_1");
		comprobar(AddAnimatedObject(escena, "puerta_2", ruta, glm::vec3(6, 3, -1.7f), -90.0f, error), "anadir puerta_2");
		if (escena.objetos.size() != 2) return;
		AnimatedObject& p1 = escena.objetos[0];
		AnimatedObject& p2 = escena.objetos[1];
		const float duracion = p1.modelo->animaciones[0].duracion;

		comprobar(p1.estado == DoorState::Cerrada && p2.estado == DoorState::Cerrada, "empiezan Cerradas");

		// --- Abrir ---
		ToggleDoor(p1);
		comprobar(p1.estado == DoorState::Abriendo, "E con la puerta cerrada: Abriendo");
		UpdateAnimatedObjects(escena, 0.1f);
		comprobar(p1.estado == DoorState::Abriendo && AnguloDeLaHoja(p1) > 5.0f, "a los 0.1 s sigue Abriendo y ya se ha movido");
		UpdateAnimatedObjects(escena, duracion + 1.0f);
		comprobar(p1.estado == DoorState::Open && p1.reproductor.tiempo == duracion, "al terminar: Abierta, en el ultimo instante");
		const float anguloAbierta = AnguloDeLaHoja(p1);
		comprobar(anguloAbierta > 80.0f, "Abierta: unos 85 grados");

		// --- Cerrar con la animacion del GLB hacia atras ---
		ToggleDoor(p1);
		comprobar(p1.estado == DoorState::Cerrando, "E con la puerta abierta: Cerrando");
		UpdateAnimatedObjects(escena, 0.0f);
		comprobar(casi(AnguloDeLaHoja(p1), anguloAbierta, 1.0e-3f), "al empezar a cerrar no hay salto");
		UpdateAnimatedObjects(escena, duracion + 1.0f);
		comprobar(p1.estado == DoorState::Cerrada && p1.reproductor.tiempo == 0.0f, "al terminar: Cerrada, en t = 0");
		comprobar(casi(AnguloDeLaHoja(p1), 0.0f, 0.5f), "Cerrada: 0 grados");

		// --- La misma animacion en los dos sentidos (sin keyframes manuales) ---
		ToggleDoor(p1);
		UpdateAnimatedObjects(escena, 0.5f);
		const float anguloAbriendoEn05 = AnguloDeLaHoja(p1);
		UpdateAnimatedObjects(escena, duracion);                    // termina de abrir
		ToggleDoor(p1);
		UpdateAnimatedObjects(escena, duracion - 0.5f);             // cierra hasta t = 0.5 exactamente
		comprobar(casi(p1.reproductor.tiempo, 0.5f), "cerrando llegamos a t = 0.5");
		comprobar(casi(AnguloDeLaHoja(p1), anguloAbriendoEn05, 1.0e-3f), "en t = 0.5 la pose es la misma abriendo que cerrando");
		UpdateAnimatedObjects(escena, duracion);                    // termina de cerrar
		comprobar(p1.estado == DoorState::Cerrada, "vuelve a Cerrada");

		// --- Invertir a mitad de movimiento, sin saltos ---
		ToggleDoor(p1);                                     // Abriendo
		UpdateAnimatedObjects(escena, 0.3f);
		const float anguloA = AnguloDeLaHoja(p1);
		ToggleDoor(p1);                                     // E mientras se abre -> Cerrando
		comprobar(p1.estado == DoorState::Cerrando, "E mientras se abre: pasa a Cerrando");
		UpdateAnimatedObjects(escena, 0.0f);
		comprobar(casi(AnguloDeLaHoja(p1), anguloA, 1.0e-3f) && casi(p1.reproductor.tiempo, 0.3f), "invertir conserva el instante: sin salto");
		UpdateAnimatedObjects(escena, 0.1f);
		comprobar(casi(p1.reproductor.tiempo, 0.2f) && p1.estado == DoorState::Cerrando, "ahora el tiempo baja");
		ToggleDoor(p1);                                     // E mientras se cierra -> Abriendo
		comprobar(p1.estado == DoorState::Abriendo, "E mientras se cierra: pasa a Abriendo");
		UpdateAnimatedObjects(escena, 0.1f);
		comprobar(casi(p1.reproductor.tiempo, 0.3f), "y vuelve a subir desde donde estaba");
		ToggleDoor(p1);                                     // otra vez -> Cerrando, hasta el final
		UpdateAnimatedObjects(escena, 5.0f);
		comprobar(p1.estado == DoorState::Cerrada && p1.reproductor.tiempo == 0.0f, "invertir hasta el principio termina en Cerrada");

		// --- Pausa: delta 0 no mueve nada ---
		ToggleDoor(p1);
		UpdateAnimatedObjects(escena, 0.0f);
		comprobar(p1.estado == DoorState::Abriendo && p1.reproductor.tiempo == 0.0f, "con delta 0 (pausa) la puerta no avanza");

		// --- La puerta_2 nunca se ha tocado ---
		comprobar(p2.estado == DoorState::Cerrada && p2.reproductor.tiempo == 0.0f && !p2.reproductor.reproduciendo, "la puerta_2 sigue Cerrada, con su reloj parado");
		comprobar(casi(AnguloDeLaHoja(p2), 0.0f, 0.5f), "la puerta_2 sigue con su pose cerrada");

		// --- Estados independientes ---
		ToggleDoor(p2);
		UpdateAnimatedObjects(escena, duracion + 1.0f);
		comprobar(p2.estado == DoorState::Open && p1.estado == DoorState::Open, "cada puerta termina en su propio estado");
		ToggleDoor(p1);
		comprobar(p1.estado == DoorState::Cerrando && p2.estado == DoorState::Open, "cerrar una no cierra la otra");

		// --- Sin animacion valida no pasa nada ---
		p2.indiceAnimacion = 99;
		ToggleDoor(p2);
		comprobar(p2.estado == DoorState::Open, "una instancia sin animacion valida no cambia de estado");
	}

	// ----------------------------------------------------------------------
	// 6. Interaccion: que puerta se activa con E
	// ----------------------------------------------------------------------
	void ProbarInteraccion(const fs::path& rutaPuerta)
	{
		AnimatedScene escena;
		std::string error;
		const std::string ruta = rutaPuerta.string();
		AddAnimatedObject(escena, "lejana", ruta, glm::vec3(6, 3, -1.7f), -90.0f, error);
		AddAnimatedObject(escena, "puerta_a", ruta, glm::vec3(6, -2.0f, -1.7f), -90.0f, error);
		AddAnimatedObject(escena, "puerta_b", ruta, glm::vec3(6, -3.4f, -1.7f), -90.0f, error);
		if (escena.objetos.size() != 3) { comprobar(false, "se crearon 3 puertas"); return; }
		AnimatedObject& lejana = escena.objetos[0];
		AnimatedObject& puertaA = escena.objetos[1];
		AnimatedObject& puertaB = escena.objetos[2];

		// El punto de la puerta es su centro, ya colocado en el mundo del juego (Z arriba).
		comprobar(casiVec(InteractionPoint(puertaA), glm::vec3(6.0f, -2.0f, -0.65f), 0.1f), "punto de interaccion = centro de la puerta en el mundo");
		const glm::vec3 cerrada = InteractionPoint(puertaA);
		puertaA.reproductor.PlayAnimation(1.0f);
		UpdateAnimatedObjects(escena, 3.0f);
		comprobar(casiVec(InteractionPoint(puertaA), cerrada), "el punto de interaccion no cambia al abrirse");
		puertaA.reproductor.tiempo = 0.0f;
		RestoreRestPose(*puertaA.modelo, puertaA.pose);
		puertaA.estado = DoorState::Cerrada;
		UpdateAnimatedObjects(escena, 0.0f);

		// Delante y cerca: se elige la puerta.
		const glm::vec3 ojos(7.5f, -2.0f, 0.0f);
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(-1, 0, 0)) == &puertaA, "delante y a menos de 2 m: se encuentra la puerta");
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(-5, 0, 0)) == &puertaA, "la direccion no necesita estar normalizada");

		// Fuera de rango, de espaldas o mirando a un lado: nada.
		comprobar(FindInteractableObject(escena, glm::vec3(9.5f, -2.0f, 0.0f), glm::vec3(-1, 0, 0)) == nullptr, "a mas de 2 m: nada");
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(1, 0, 0)) == nullptr, "de espaldas a la puerta: nada");
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(0, 1, 0)) == nullptr, "mirando a un lado (90 grados): nada");
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(0, 0, 0)) == nullptr, "direccion nula: nada");
		comprobar(FindInteractableObject(escena, ojos, glm::vec3(-1, 0, 0), 1.0f) == nullptr, "con radio de 1 m ya no llega");

		// Dos puertas cercanas: gana la que se mira, y solo se activa esa.
		const glm::vec3 entre(7.5f, -2.7f, 0.0f);
		const glm::vec3 haciaA = InteractionPoint(puertaA) - entre;
		const glm::vec3 haciaB = InteractionPoint(puertaB) - entre;
		comprobar(glm::distance(entre, InteractionPoint(puertaA)) < RADIO_INTERACCION && glm::distance(entre, InteractionPoint(puertaB)) < RADIO_INTERACCION,
			"las dos puertas estan al alcance");
		comprobar(FindInteractableObject(escena, entre, haciaA) == &puertaA, "mirando a la puerta_a se elige la puerta_a");
		comprobar(FindInteractableObject(escena, entre, haciaB) == &puertaB, "mirando a la puerta_b se elige la puerta_b");

		// Mirando entre las dos, AMBAS estan dentro del cono: gana la mas centrada en la mirada.
		const glm::vec3 casiHaciaB(-1.5f, -0.2f, -0.65f);
		const glm::vec3 casiHaciaA(-1.5f, 0.2f, -0.65f);
		const float cosenoA = glm::dot(glm::normalize(casiHaciaB), glm::normalize(haciaA));
		const float cosenoB = glm::dot(glm::normalize(casiHaciaB), glm::normalize(haciaB));
		comprobar(cosenoA >= COSENO_MINIMO_INTERACCION && cosenoB >= COSENO_MINIMO_INTERACCION, "con esta mirada las dos puertas son candidatas");
		comprobar(FindInteractableObject(escena, entre, casiHaciaB) == &puertaB, "dos candidatas: gana la puerta_b, mas centrada");
		comprobar(FindInteractableObject(escena, entre, casiHaciaA) == &puertaA, "dos candidatas: gana la puerta_a, mas centrada");

		AnimatedObject* usada = InteractWithObject(escena, entre, haciaA);
		comprobar(usada == &puertaA && puertaA.estado == DoorState::Abriendo, "E: la puerta elegida empieza a abrirse");
		comprobar(puertaB.estado == DoorState::Cerrada && lejana.estado == DoorState::Cerrada, "las demas puertas no se activan");
		usada = InteractWithObject(escena, entre, haciaA);
		comprobar(usada == &puertaA && puertaA.estado == DoorState::Cerrando, "E otra vez: invierte el sentido");
		comprobar(InteractWithObject(escena, entre, glm::vec3(1, 0, 0)) == nullptr, "E sin puerta delante no hace nada");

		// Una instancia sin animacion valida no es interactuable.
		puertaB.indiceAnimacion = 99;
		comprobar(FindInteractableObject(escena, entre, haciaB) != &puertaB, "sin animacion valida no se puede interactuar");

		// Misma funcion con una "mano" que no es la camara: posicion y direccion cualesquiera.
		const glm::vec3 mano(6.5f, -2.0f, -0.65f);
		comprobar(FindInteractableObject(escena, mano, glm::vec3(-1, 0, 0)) == &puertaA, "sirve con la posicion y direccion de un mando");

		// Cuando se puede usar E.
		comprobar(CanInteract(false, true, false), "E permitida en juego normal");
		comprobar(!CanInteract(true, true, false), "E bloqueada con el juego en pausa");
		comprobar(!CanInteract(false, false, false), "E bloqueada sin el foco de la ventana");
		comprobar(!CanInteract(false, true, true), "E bloqueada mientras se escribe en ImGui");
	}

	void ProbarCentroDelModelo(const fs::path& rutaPuerta)
	{
		GlbModel modelo;
		std::string error;
		if (!LoadGlbModel(rutaPuerta, modelo, error)) { comprobar(false, "cargar la puerta para el centro"); return; }
		const glm::vec3 centro = ComputeModelCenter(modelo, CreateRestPose(modelo));
		// Marco de 1 m de ancho y 2.1 m de alto: centro en x = 0, y = 1.05 (Y arriba en glTF).
		comprobar(casiVec(centro, glm::vec3(0.0f, 1.05f, 0.0f), 0.1f), "el centro de la puerta esta a la mitad de su altura");
		comprobar(casiVec(ComputeModelCenter(GlbModel(), GlbPose()), glm::vec3(0.0f)), "un modelo vacio tiene el centro en el origen");
	}

	void ProbarErrores(const fs::path& carpeta)
	{
		GlbModel modelo;
		std::string error;
		comprobar(!LoadGlbModel(carpeta / "no_existe.glb", modelo, error) && !error.empty(), "archivo inexistente: error");

		const fs::path texto = carpeta / "falso.glb";
		std::ofstream(texto) << "esto no es un glb";
		comprobar(!LoadGlbModel(texto, modelo, error) && !error.empty(), "archivo que no es glb: error");

		// Un modelo ya cargado no se estropea si una carga posterior falla.
		EscribirGlbCajon(carpeta / "bueno.glb", "LINEAR");
		comprobar(LoadGlbModel(carpeta / "bueno.glb", modelo, error), "carga previa correcta");
		comprobar(!LoadGlbModel(texto, modelo, error) && modelo.nodos.size() == 2, "una carga fallida no borra el modelo anterior");
	}
}

int main()
{
	ProbarInterpolacion();
	ProbarReproductor();
	ProbarReproductorHaciaAtras();
	ProbarConversionDeCoordenadas();

	ProbarPuertaReal(fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets" / "animations" / "Puerta.glb");
	ProbarPuertaLaboratorio(fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets" / "animations" / "Puerta_Lab.glb");

	const auto unico = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path carpeta = fs::temp_directory_path() / ("EscapeTheUAB-GlbTests-" + std::to_string(unico));
	fs::create_directories(carpeta);
	ProbarGlbInventado(carpeta);
	ProbarObjetosAnimados(fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets" / "animations" / "Puerta.glb", carpeta);
	{
		const fs::path puerta = fs::path(ESCAPETHEUAB_SOURCE_DIR) / "assets" / "animations" / "Puerta.glb";
		ProbarCentroDelModelo(puerta);
		ProbarEstadosDePuerta(puerta);
		ProbarInteraccion(puerta);
	}
	ProbarErrores(carpeta);
	std::error_code ec;
	fs::remove_all(carpeta, ec);

	if (fallos == 0) std::cout << "GlbTests: todo correcto\n";
	return fallos == 0 ? 0 : 1;
}
