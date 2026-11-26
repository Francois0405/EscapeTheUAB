#version 330 core
// Posprocesado retro. Trabaja sobre los valores ya codificados para pantalla (el juego no usa
// GL_FRAMEBUFFER_SRGB), asi que no se aplica ninguna correccion gamma adicional.
// Todo el ruido depende del pixel INTERNO y del canal, nunca del tiempo: no parpadea.
// La formula de cuantizacion y las matrices Bayer coinciden con src/rendering/RetroSettings.h.
in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uScene;
uniform ivec2 uInternalSize;
uniform int uLevels;
uniform int uDitherMode;   // 0 ninguno, 1 Bayer 4x4, 2 Bayer 8x8, 3 hash espacial
uniform float uDitherStrength;
uniform float uSaturation;
uniform float uContrast;
uniform float uExposure;
uniform float uGrain;
uniform float uVignette;
uniform float uGrading;

const int bayer4[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
const int bayer2[4] = int[4](0, 2, 3, 1);

int bayer4Value(ivec2 p) { return bayer4[(p.y & 3) * 4 + (p.x & 3)]; }

float hash(ivec2 p, int channel)
{
	uint h = uint(p.x) * 374761393u + uint(p.y) * 668265263u + uint(channel) * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return float(h & 65535u) / 65536.0;
}

// Cada canal usa un desplazamiento distinto del patron: de ahi el punteado cromatico.
float ditherOffset(ivec2 p, int channel)
{
	const ivec2 shift[3] = ivec2[3](ivec2(0, 0), ivec2(2, 1), ivec2(1, 3));
	if (uDitherMode == 1) return (float(bayer4Value(p + shift[channel])) + 0.5) / 16.0 - 0.5;
	if (uDitherMode == 2)
	{
		ivec2 q = p + shift[channel] * 2 + ivec2(channel);
		int v = 4 * bayer4Value(q) + bayer2[((q.y >> 2) & 1) * 2 + ((q.x >> 2) & 1)];
		return (float(v) + 0.5) / 64.0 - 0.5;
	}
	if (uDitherMode == 3) return hash(p, channel) - 0.5;
	return 0.0;
}

void main()
{
	ivec2 p = clamp(ivec2(floor(vUv * vec2(uInternalSize))), ivec2(0), uInternalSize - 1);
	vec3 c = texelFetch(uScene, p, 0).rgb * uExposure;
	c = (c - 0.5) * uContrast + 0.5;
	float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
	c = mix(vec3(luma), c, uSaturation);

	// Sombras ligeramente frias y luces calidas; el negro puro no se levanta.
	float warm = smoothstep(0.15, 0.85, luma);
	c *= mix(vec3(1.0 - uGrading, 1.0, 1.0 + uGrading), vec3(1.0 + uGrading, 1.0, 1.0 - uGrading), warm);

	float edge = smoothstep(0.35, 0.75, distance(vUv, vec2(0.5)));
	c *= 1.0 - uVignette * edge;

	// Granulado estatico, concentrado en sombras y medios y proporcional para no aclarar los negros.
	float shadow = (1.0 - clamp(luma, 0.0, 1.0)) * smoothstep(0.0, 0.08, luma);
	vec3 noise = vec3(hash(p, 3), hash(p, 4), hash(p, 5)) - 0.5;
	c += noise * (2.0 * uGrain * shadow);

	c = clamp(c, 0.0, 1.0);
	vec3 offset = vec3(ditherOffset(p, 0), ditherOffset(p, 1), ditherOffset(p, 2)) * uDitherStrength;
	float levels = float(uLevels);
	c = clamp(floor(c * levels + 0.5 + offset), 0.0, levels) / levels;
	fragColor = vec4(c, 1.0);
}
