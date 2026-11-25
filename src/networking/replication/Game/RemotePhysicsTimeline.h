#pragma once

#include "src/networking/replication/NetJuegoLogica.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>

namespace netgame::timeline
{
	struct RemotePhysicsSample
	{
		double time = 0.0;
		net::DynamicStateEntry state;
	};

	using RemotePhysicsBuffers = std::map<uint32_t, std::deque<RemotePhysicsSample>>;

	struct RemotePhysicsPose
	{
		glm::vec3 position;
		glm::quat rotation;
	};

	inline float RemotePhysicsBlendFactor(float delta)
	{
		return 1.0f - std::exp(-18.0f * delta);
	}

	inline RemotePhysicsPose BlendRemotePhysicsPose(const glm::vec3& currentPosition,
		const glm::quat& currentRotation, const glm::vec3& targetPosition,
		const glm::quat& targetRotation, float delta)
	{
		const float blend = RemotePhysicsBlendFactor(delta);
		return { glm::mix(currentPosition, targetPosition, blend),
			glm::slerp(currentRotation, targetRotation, blend) };
	}

	inline void StoreRemotePhysicsSample(RemotePhysicsBuffers& buffers,
		const net::DynamicStateEntry& state, double now)
	{
		if (!IsValidPhysicsState(state)) return;
		auto& buffer = buffers[state.objectId];
		if (!buffer.empty() && !net::SeqNewer(state.tick, buffer.back().state.tick)) return;
		const double sampleTime = buffer.empty() ? now : NextSnapshotTime(
			buffer.back().time, buffer.back().state.tick, state.tick);
		buffer.push_back({ sampleTime, state });
		while (buffer.size() > 8) buffer.pop_front();
	}

	// The caller retains the non-empty buffer guard before calling this helper.
	inline net::DynamicStateEntry InterpolateRemotePhysicsState(
		const std::deque<RemotePhysicsSample>& buffer, double now)
	{
		net::DynamicStateEntry result = buffer.back().state;
		const double targetTime = now - 0.12;
		if (buffer.size() > 1)
		{
			for (std::size_t i = 1; i < buffer.size(); ++i)
			{
				if (targetTime > buffer[i].time) continue;
				const auto& first = buffer[i - 1]; const auto& second = buffer[i];
				const float t = second.time > first.time ? std::clamp(
					static_cast<float>((targetTime - first.time) / (second.time - first.time)), 0.0f, 1.0f) : 1.0f;
				const glm::vec3 p = glm::mix(PositionOfPose(first.state.transform), PositionOfPose(second.state.transform), t);
				const glm::quat q = glm::slerp(RotationOfPose(first.state.transform), RotationOfPose(second.state.transform), t);
				result.transform = ToTransform(p, q);
				return result;
			}
		}
		// Do not extrapolate rigid-body simulation; hold the last snapshot if no later one is available.
		return result;
	}

	inline void ApplyReliableFinalPhysicsState(RemotePhysicsBuffers& buffers,
		const net::DynamicStateEntry& state, double now)
	{
		auto existing = buffers.find(state.objectId);
		const bool hasSample = existing != buffers.end() && !existing->second.empty();
		const uint32_t currentTick = hasSample ? existing->second.back().state.tick : 0;
		if (IsNewerPhysicsState(state.tick, hasSample, currentTick))
		{
			buffers.erase(state.objectId);
			StoreRemotePhysicsSample(buffers, state, now);
		}
	}
}
