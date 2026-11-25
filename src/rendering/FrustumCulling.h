#pragma once
#include <glm/glm.hpp>
#include <array>

// Reject a transformed local AABB only when all eight corners are outside the
// same homogeneous clip plane. This remains conservative for rotated/scaled boxes.
inline bool CajaFueraDeFrustum(const glm::mat4& viewProjection, const glm::mat4& model,
	const glm::vec3& minimum, const glm::vec3& maximum)
{
	std::array<glm::vec4, 8> clip{};
	for (unsigned int i = 0; i < 8; ++i)
	{
		const glm::vec3 p((i & 1) ? maximum.x : minimum.x,
			(i & 2) ? maximum.y : minimum.y, (i & 4) ? maximum.z : minimum.z);
		clip[i] = viewProjection * model * glm::vec4(p, 1.0f);
	}
	for (int plane = 0; plane < 6; ++plane)
	{
		bool allOutside = true;
		for (const glm::vec4& p : clip)
		{
			const bool outside = plane == 0 ? p.x < -p.w : plane == 1 ? p.x > p.w :
				plane == 2 ? p.y < -p.w : plane == 3 ? p.y > p.w :
				plane == 4 ? p.z < -p.w : p.z > p.w;
			if (!outside) { allOutside = false; break; }
		}
		if (allOutside) return true;
	}
	return false;
}
