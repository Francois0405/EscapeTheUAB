#include "src/gameplay/hirity/HiritiRuntime.h"
#include "src/gameplay/hirity/HiritiSoundVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>

namespace game::hiriti
{
	namespace
	{
		float radioHiriti(const SceneObject& objeto)
		{
			return std::max({ std::abs(objeto.escala.x), std::abs(objeto.escala.y), std::abs(objeto.escala.z) });
		}

		float volumenSonidoHiriti(const HiritiContext& context, float volumenBase)
		{
			auto& sceneObjects = context.sceneObjects;
			auto& physicsScene = context.physicsScene;
			auto& physicsWorld = context.physicsWorld;

			if (!physicsWorld.HasPlayerBody() || !physicsScene) return volumenBase;

			glm::vec3 posicionHiriti(0.0f);
			const auto hiriti = std::find_if(sceneObjects.begin(), sceneObjects.end(),
				[](const SceneObject& objeto) { return objeto.id == "hiriti"; });
			if (hiriti == sceneObjects.end()) return volumenBase;

			posicionHiriti = hiriti->posicion;
			const int indice = static_cast<int>(std::distance(sceneObjects.begin(), hiriti));
			physicsScene->DynamicObjectPosition(indice, posicionHiriti);

			const glm::vec3 posicionJugador = physicsWorld.PlayerFeetPosition() +
				glm::vec3(0.0f, 0.0f, physicsWorld.PlayerEyeHeight());
			const float distancia = glm::length(posicionHiriti - posicionJugador);
			return AttenuateVolumeByDistance(volumenBase, distancia);
		}

		void reproducirSonidoAparicionHiriti(const HiritiContext& context)
		{
			static const std::vector<std::string> sonidos = []
			{
				std::vector<std::string> encontrados;
				const std::filesystem::path carpeta("assets/audio/hirity/appear");
				std::error_code error;
				for (std::filesystem::directory_iterator entrada(carpeta, error), fin;
					!error && entrada != fin; entrada.increment(error))
				{
					if (entrada->is_regular_file(error) && entrada->path().extension() == ".ogg")
						encontrados.push_back(entrada->path().string());
				}
				if (error)
					std::cerr << "No se pudieron enumerar los sonidos de aparicion de Hiriti: "
						<< error.message() << '\n';
				if (encontrados.empty())
					encontrados.push_back("assets/audio/hirity/appear/anotopat.ogg");
				std::sort(encontrados.begin(), encontrados.end());
				return encontrados;
			}();

			static std::mt19937 generador(std::random_device{}());
			std::uniform_int_distribution<std::size_t> elegir(0, sonidos.size() - 1);
			PlayHiritiSound(context, sonidos[elegir(generador)], 1.6f);
		}

		bool reaparecerHiritiDetrasDelJugador(const HiritiContext& context, int indice, const SceneObject& objeto,
			const glm::vec3& piesJugador, glm::vec3& centroAparicion)
		{
			auto& physicsScene = context.physicsScene;

			const glm::vec2 frenteSinNormalizar = context.cameraHorizontalForward();
			const float longitudFrente = glm::length(frenteSinNormalizar);
			if (longitudFrente < 1.0e-4f) return false;
			const glm::vec2 atras = -frenteSinNormalizar / longitudFrente;
			const float radio = std::max(0.15f, radioHiriti(objeto));
			constexpr float angulos[] = { 0.0f, 0.5235987756f, -0.5235987756f,
				1.0471975512f, -1.0471975512f };
			constexpr float distancias[] = { 2.4f, 2.9f, 3.4f };

			for (float distancia : distancias)
			{
				for (float angulo : angulos)
				{
					const glm::vec2 direccion(
						atras.x * std::cos(angulo) - atras.y * std::sin(angulo),
						atras.x * std::sin(angulo) + atras.y * std::cos(angulo));
					const glm::vec3 candidato = piesJugador + glm::vec3(
						direccion.x * distancia, direccion.y * distancia, radio + 0.04f);
					const float distanciaObstaculo = physicsScene->DynamicObjectObstacleDistanceOnRay(
						indice, piesJugador + glm::vec3(0.0f, 0.0f, radio + 0.04f),
						glm::vec3(direccion.x, direccion.y, 0.0f), distancia);
					if (distanciaObstaculo < distancia - radio * 0.5f ||
						!physicsScene->CanPlaceDynamicObject(indice, candidato))
						continue;
					centroAparicion = candidato;
					return physicsScene->TeleportDynamicObject(indice, centroAparicion);
				}
			}
			return false;
		}
	}

	void PlayHiritiSound(const HiritiContext& context, const std::string& ruta, float volumenBase)
	{
		context.audioManager.playOneShot(ruta, volumenSonidoHiriti(context, volumenBase));
	}

	void UpdateHiriti(const HiritiContext& context, bool puedeDescubrir, bool puedeProcesarColisiones, float delta)
	{
		auto& hiritiAnimator = context.hiritiAnimation;
		auto& sceneObjects = context.sceneObjects;
		auto& physicsScene = context.physicsScene;
		auto& physicsWorld = context.physicsWorld;
		auto& networkGame = context.session;
		auto& hiritiFollowPlayerEnabled = context.hiritiFollowEnabled;
		auto& hiritiFollowStopDistance = context.hiritiStopDistance;
		auto& hiritiFollowMaxSpeed = context.hiritiMaxSpeed;
		auto& gazeOriginAndDirection = context.lookRay;
		auto& grabTraceEnabled = context.grabDiagnosticsEnabled;
		auto& emitGrabTrace = context.emitGrabDiagnostic;
		auto& traceGrab = context.traceGrab;

		const auto hiriti = std::find_if(sceneObjects.begin(), sceneObjects.end(),
			[](const SceneObject& objeto) { return objeto.id == "hiriti"; });
		if (hiriti == sceneObjects.end())
		{
			hiritiAnimator.ResetState();
			return;
		}

		if (puedeDescubrir && hiritiAnimator.UpdateGrabMe(delta))
			PlayHiritiSound(context, "assets/audio/hirity/story/agarrame.ogg");

		glm::vec3 origen(0.0f), direccion(0.0f);
		if (puedeDescubrir) gazeOriginAndDirection(origen, direccion);
		const int indice = static_cast<int>(std::distance(sceneObjects.begin(), hiriti));
		glm::vec3 velocidadHiriti(0.0f);
		physicsScene->DynamicObjectVelocity(indice, velocidadHiriti);
		const uint32_t idHiriti = netgame::HashId(hiriti->id);
		const bool propietarioRed = networkGame.IsActive() &&
			networkGame.DynamicOwner(idHiriti) != netgame::kSinDueno;
		const bool jugadorManipulaHiriti = physicsScene->HeldDynamicObject(indice) || propietarioRed;
		const bool puedeSeguir = hiritiAnimator.UpdateFollowSuspension(
			jugadorManipulaHiriti, velocidadHiriti, puedeProcesarColisiones ? delta : 0.0f);
		const int varianteDanio = hiritiAnimator.UpdateContact(
			puedeProcesarColisiones && physicsScene->HasDynamicObjectContact(indice));
		if (varianteDanio >= 0)
		{
			traceGrab("hiriti-contact", idHiriti, networkGame.LocalPeerId(),
				networkGame.DynamicOwner(idHiriti), hiriti->posicion, velocidadHiriti,
				varianteDanio == 0 ? "contact-damage-audio" : "contact-damage-audio-long");
			PlayHiritiSound(context, varianteDanio == 0
				? "assets/audio/hirity/damage/damagehirity.ogg"
				: "assets/audio/hirity/damage/damegehiritylong.ogg");
		}

		glm::vec3 centro = hiriti->posicion;
		physicsScene->DynamicObjectPosition(indice, centro);
		if (grabTraceEnabled())
		{
			static double ultimaMuestraHiriti = 0.0;
			if (context.clockSeconds() - ultimaMuestraHiriti >= 0.5)
			{
				const net::PeerId propietario = networkGame.IsActive()
					? networkGame.DynamicOwner(idHiriti) : netgame::kSinDueno;
				std::ostringstream registro;
				registro << "[grab] hiriti id=" << idHiriti << " owner=" << propietario
					<< " body=" << (networkGame.IsClient() ? "kinematic-replica" : "authoritative-dynamic")
					<< " held=" << (jugadorManipulaHiriti ? "yes" : "no")
					<< " tracking=" << (networkGame.IsClient() ? "host-authoritative-unknown" :
						(puedeSeguir ? "eligible" : "suspended-or-stabilizing"))
					<< " followTarget=" << (networkGame.IsHosting() ? "host-local-player" : (networkGame.IsClient() ? "host-authoritative" : "offline-local-player"))
					<< " respawnElapsed=" << hiritiAnimator.TimeSinceRespawn()
					<< " pos=(" << centro.x << ',' << centro.y << ',' << centro.z << ") velocity=("
					<< velocidadHiriti.x << ',' << velocidadHiriti.y << ',' << velocidadHiriti.z << ')';
				emitGrabTrace(registro.str());
				ultimaMuestraHiriti = context.clockSeconds();
			}
		}
		bool reaparecio = false;
		if (puedeProcesarColisiones && physicsWorld.HasPlayerBody())
		{
			const glm::vec3 piesJugador = physicsWorld.PlayerFeetPosition();
			const float distanciaAlJugador = glm::length(glm::vec2(
				piesJugador.x - centro.x, piesJugador.y - centro.y));
			const bool interaccionJugador = jugadorManipulaHiriti || !puedeSeguir;
			if (hiritiAnimator.UpdateRespawnTimer(
				distanciaAlJugador > game::HiritiAnimator::DistanciaParaReaparecer,
				interaccionJugador, delta))
			{
				glm::vec3 centroAparicion(0.0f);
				if (reaparecerHiritiDetrasDelJugador(context, indice, *hiriti, piesJugador, centroAparicion))
				{
					centro = centroAparicion;
					reaparecio = true;
					hiritiAnimator.RegisterRespawn();
					traceGrab("hiriti-respawn", idHiriti, net::kHostPeerId,
						networkGame.DynamicOwner(idHiriti), centro, centroAparicion,
						"host-authoritative-respawn-after-distance-and-cooldown-conditions");
					reproducirSonidoAparicionHiriti(context);
				}
			}
		}
		if (!reaparecio && hiritiFollowPlayerEnabled && puedeProcesarColisiones && physicsWorld.HasPlayerBody() &&
			puedeSeguir && delta > 0.0f)
		{
			const glm::vec3 piesJugador = physicsWorld.PlayerFeetPosition();
			const glm::vec2 haciaJugador(piesJugador.x - centro.x, piesJugador.y - centro.y);
			const float distanciaJugador = glm::length(haciaJugador);
			const glm::vec2 direccionObjetivo = distanciaJugador > 1.0e-4f
				? haciaJugador / distanciaJugador : glm::vec2(0.0f);
			constexpr float angulosSonda[] = { 0.0f, 0.7853981634f, -0.7853981634f,
				1.5707963268f, -1.5707963268f };
			std::array<float, 5> distanciasObstaculos{};
			for (std::size_t i = 0; i < distanciasObstaculos.size(); ++i)
			{
				const float coseno = std::cos(angulosSonda[i]);
				const float seno = std::sin(angulosSonda[i]);
				const glm::vec3 direccion(
					direccionObjetivo.x * coseno - direccionObjetivo.y * seno,
					direccionObjetivo.x * seno + direccionObjetivo.y * coseno,
					0.0f);
				distanciasObstaculos[i] = physicsScene->DynamicObjectObstacleDistanceOnRay(
					indice, centro, direccion, game::HiritiAnimator::AlcanceAnticipacionObstaculos);
			}
			const glm::vec3 direccionSeguimiento = game::HiritiAnimator::ComputeFollowDirection(
				centro, piesJugador, distanciasObstaculos);
			const glm::vec3 aceleracion = game::HiritiAnimator::ComputeFollowAcceleration(
				centro, piesJugador, velocidadHiriti, direccionSeguimiento,
				hiritiFollowStopDistance, hiritiFollowMaxSpeed, 3.2f);
			physicsScene->ApplyDynamicObjectHorizontalAcceleration(
				indice, aceleracion, std::min(delta, 0.05f));
		}
		// La primera mirada dispara el saludo y el impulso inicial.
		if (hiritiAnimator.Advance(puedeDescubrir, origen, direccion, centro, radioHiriti(*hiriti)))
		{
			static std::mt19937 generador(std::random_device{}());
			static std::uniform_int_distribution<int> probabilidadGallego(1, 5);
			const char* saludo = probabilidadGallego(generador) == 1
				? "assets/audio/hirity/story/holahiritygallego.ogg"
				: "assets/audio/hirity/story/holahirity.ogg";
			PlayHiritiSound(context, saludo);

			// La primera mirada y su sonido pueden ser locales, pero solo el host/offline impulsa el cuerpo.
			if (puedeProcesarColisiones &&
				!physicsScene->PushDynamicObject(indice, glm::vec3(0.0f, 0.0f, 0.75f)))
				std::cerr << "No se pudo dar el impulso inicial a Hiriti.\n";
			std::cout << "Has descubierto a Hiriti.\n";
		}
	}

	void UpdateAmbientAudio(const HiritiContext& context, bool activo, float delta)
	{
		auto& hiritiAudioTimer = context.hiritiAudioTimer;
		auto& hiritiFollowPlayerEnabled = context.hiritiFollowEnabled;

		static std::mt19937 generador(std::random_device{}());
		static std::uniform_int_distribution<int> tipoAmbiente(0, 1);
		static std::uniform_int_distribution<int> tipoBuscando(0, 1);
		if (!activo) hiritiFollowPlayerEnabled = false;
		switch (hiritiAudioTimer.Advance(activo, delta))
		{
		case game::HiritiAudioTimer::Evento::Ambiente:
			hiritiFollowPlayerEnabled = false;
			PlayHiritiSound(context, tipoAmbiente(generador) == 0
				? "assets/audio/hirity/ambient/cough.ogg"
				: "assets/audio/hirity/ambient/pasosfuertes.ogg");
			break;
		case game::HiritiAudioTimer::Evento::Buscando:
			hiritiFollowPlayerEnabled = true;
			PlayHiritiSound(context, tipoBuscando(generador) == 0
				? "assets/audio/hirity/searching/heydondeestashirity.ogg"
				: "assets/audio/hirity/searching/venaquihirity.ogg");
			break;
		case game::HiritiAudioTimer::Evento::Ninguno:
			break;
		}
	}
}
