#include "PlacementValidator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor
{
	OrientedBox TransformBox(const glm::mat4& modelo, const glm::vec3& centroLocal, const glm::vec3& mitadLocal)
	{
		OrientedBox caja;
		caja.centro = glm::vec3(modelo * glm::vec4(centroLocal, 1.0f));
		for (int eje = 0; eje < 3; ++eje)
		{
			const glm::vec3 columna(modelo[eje]);
			const float escala = glm::length(columna);
			if (!(escala > 1.0e-8f) || !std::isfinite(escala) || !std::isfinite(mitadLocal[eje])) return caja;
			caja.ejes[static_cast<std::size_t>(eje)] = columna / escala;
			caja.mitad[eje] = std::abs(mitadLocal[eje]) * escala;
		}
		caja.valida = std::isfinite(caja.centro.x) && std::isfinite(caja.centro.y) && std::isfinite(caja.centro.z);
		return caja;
	}

	float ObbPenetration(const OrientedBox& a, const OrientedBox& b, float toleranciaContacto)
	{
		if (!a.valida || !b.valida) return 0.0f;
		const glm::vec3 delta = b.centro - a.centro;
		float profundidadMinima = std::numeric_limits<float>::max();
		auto probarEje = [&](glm::vec3 eje)
		{
			const float longitud = glm::length(eje);
			if (longitud < 1.0e-6f) return true;
			eje /= longitud;
			float radioA = 0.0f, radioB = 0.0f;
			for (int i = 0; i < 3; ++i)
			{
				radioA += a.mitad[i] * std::abs(glm::dot(eje, a.ejes[static_cast<std::size_t>(i)]));
				radioB += b.mitad[i] * std::abs(glm::dot(eje, b.ejes[static_cast<std::size_t>(i)]));
			}
			const float solape = radioA + radioB - std::abs(glm::dot(delta, eje));
			if (solape <= toleranciaContacto) return false;
			profundidadMinima = std::min(profundidadMinima, solape);
			return true;
		};

		for (int i = 0; i < 3; ++i) if (!probarEje(a.ejes[static_cast<std::size_t>(i)])) return 0.0f;
		for (int i = 0; i < 3; ++i) if (!probarEje(b.ejes[static_cast<std::size_t>(i)])) return 0.0f;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				if (!probarEje(glm::cross(a.ejes[static_cast<std::size_t>(i)], b.ejes[static_cast<std::size_t>(j)]))) return 0.0f;
		return profundidadMinima;
	}

	bool CalculateStagingOffsetX(const std::vector<OrientedBox>& propuesta,
		const std::vector<OrientedBox>& obstaculos, float margen, float& desplazamiento)
	{
		desplazamiento = 0.0f;
		if (!std::isfinite(margen) || margen < 0.0f) return false;

		auto mitadEnX = [](const OrientedBox& caja, float& mitad)
		{
			if (!caja.valida || !std::isfinite(caja.centro.x)) return false;
			mitad = 0.0f;
			for (int eje = 0; eje < 3; ++eje)
			{
				const glm::vec3& direccion = caja.ejes[static_cast<std::size_t>(eje)];
				if (!std::isfinite(direccion.x) || !std::isfinite(caja.mitad[eje]) || caja.mitad[eje] < 0.0f)
					return false;
				mitad += std::abs(direccion.x) * caja.mitad[eje];
			}
			return std::isfinite(mitad);
		};

		float minimoPropuesta = std::numeric_limits<float>::max();
		bool hayPropuesta = false;
		for (const OrientedBox& caja : propuesta)
		{
			float mitad = 0.0f;
			if (!mitadEnX(caja, mitad)) continue;
			minimoPropuesta = std::min(minimoPropuesta, caja.centro.x - mitad);
			hayPropuesta = true;
		}

		float maximoObstaculos = std::numeric_limits<float>::lowest();
		bool hayObstaculos = false;
		for (const OrientedBox& caja : obstaculos)
		{
			float mitad = 0.0f;
			if (!mitadEnX(caja, mitad)) continue;
			maximoObstaculos = std::max(maximoObstaculos, caja.centro.x + mitad);
			hayObstaculos = true;
		}
		if (!hayPropuesta || !hayObstaculos) return false;

		desplazamiento = std::max(0.0f, maximoObstaculos + margen - minimoPropuesta);
		return std::isfinite(desplazamiento);
	}

	PlacementResult ValidatePlacement(const std::vector<OrientedBox>& propuesta,
		const std::vector<OrientedBox>& anterior, const std::vector<PlacementObstacle>& obstaculos,
		bool permitirArquitectura, float toleranciaContacto)
	{
		for (std::size_t indiceCaja = 0; indiceCaja < propuesta.size(); ++indiceCaja)
			for (const PlacementObstacle& obstaculo : obstaculos)
			{
				if (!obstaculo.caja.valida || (permitirArquitectura && obstaculo.arquitectura)) continue;
				const float nuevaPenetracion = ObbPenetration(propuesta[indiceCaja], obstaculo.caja, toleranciaContacto);
				if (nuevaPenetracion <= 0.0f) continue;
				const float penetracionAnterior = indiceCaja < anterior.size()
					? ObbPenetration(anterior[indiceCaja], obstaculo.caja, toleranciaContacto) : 0.0f;
				if (penetracionAnterior > 0.0f && nuevaPenetracion <= penetracionAnterior + toleranciaContacto) continue;
				return { false, obstaculo.nombre.empty() ? obstaculo.id : obstaculo.nombre + " (" + obstaculo.id + ")" };
			}
		return {};
	}
}
