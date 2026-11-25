#pragma once

// Reglas puras de la sesion de red del juego: sin escena, fisica, ventana ni estado.
#include "src/networking/replication/NetJuegoLogica.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace netgame::session
{
	// El host solo acepta un rayo de agarre remoto que salga de la cabeza o la mano del peer y que
	// apunte aproximadamente hacia donde apunta su mano.
	inline bool RemoteGrabRayMatchesPose(const glm::vec3& origin, const glm::vec3& direction,
		const net::PoseEntry& pose)
	{
		const glm::vec3 hand = PositionOfPose(pose.rightHand);
		const glm::vec3 head = PositionOfPose(pose.head);
		const bool originValid = glm::distance(origin, hand) <= 0.85f || glm::distance(origin, head) <= 0.85f;
		const glm::vec3 forward = RotationOfPose(pose.rightHand) * glm::vec3(0, 1, 0);
		const float alignment = glm::dot(glm::normalize(forward), direction);
		return originValid && std::isfinite(alignment) && alignment >= 0.60f;
	}

	// En escritorio el rayo sale de la cabeza; en VR, del mando.
	inline bool GrabAnchorsToHead(const glm::vec3& origin, const net::PoseEntry& pose)
	{
		return glm::distance(origin, PositionOfPose(pose.head)) <= glm::distance(origin, PositionOfPose(pose.rightHand));
	}

	// Carga [0,1] equivalente a la velocidad con la que un peer ha soltado un objeto.
	inline float ThrowChargeFromReleaseVelocity(const glm::vec3& velocity)
	{
		return std::clamp((glm::length(velocity) - 3.0f) / 12.0f, 0.0f, 1.0f);
	}

	// Donde nace el objeto compartido de prueba: 2 m delante en el plano XY y algo por debajo de los ojos.
	inline glm::vec3 SharedObjectSpawnPosition(const glm::vec3& eye, const glm::vec3& viewForward)
	{
		glm::vec3 forward(viewForward.x, viewForward.y, 0.0f);
		forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0.0f, 1.0f, 0.0f);
		return eye + forward * 2.0f - glm::vec3(0.0f, 0.0f, 0.5f);
	}

	// Tick en milisegundos de los snapshots fisicos del host (da la vuelta en 32 bits).
	inline uint32_t PhysicsSnapshotTick(double nowSeconds)
	{
		return static_cast<uint32_t>(static_cast<uint64_t>(nowSeconds * 1000.0));
	}

	// bit 0: cuerpo activo; bit 1: tiene propietario.
	inline uint8_t PhysicsStateFlags(bool active, net::PeerId owner)
	{
		return static_cast<uint8_t>((active ? 1 : 0) | (owner != kSinDueno ? 2 : 0));
	}
}
