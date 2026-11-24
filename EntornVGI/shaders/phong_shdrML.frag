//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (EE-UAB)
//******** Entorn b�sic VS2026 amb interf�cie MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// phong_shdrML.frag: Fragment Program en GLSL en versi� OpenGL 3.3 o 4.60 per a implementar:
//     a) Iluminaci� de Gouraud
//     b) Fonts de llum puntuals o direccionals
//     c) Fonts de llum restringides
//     d) Atenuaci� de fonts de llum
//     e) Modus d'assignaci� de textura MODULATE o DECAL
//     f) Calcul component a de transpar�ncia segons color sigui provinent de material o de VAO
//     g) Algorisme incremental iluminaci� Phong encapsulat en phongModel()

//#version 330 core
#version 330 core

#define MaxLights 17
uniform bool uMaterialEmission;
#define MaxOccluders 12
#define MaxIndirectOccluders 24
uniform int uOccluderCount;
uniform mat4 uOccluderWorldToLocal[MaxOccluders];
uniform vec3 uOccluderHalfSize[MaxOccluders];
uniform int uIndirectOccluderCount;
uniform mat4 uIndirectOccluderWorldToLocal[MaxIndirectOccluders];
uniform vec3 uIndirectOccluderHalfSize[MaxIndirectOccluders];
uniform float uLampEmissionScale;

// ---- L14- Estructures Font de LLum i Material (coeficients reflectivitat).
struct Light
{	bool sw_light;		// Booleana que indica si llum encesa (TRUE) o apagada (FALSE).
	vec4 position;		// Posici� de la font de llum, en Coord. M�n o C�mera (Punt de Vista).
	vec4 ambient;		// Intensitat (r,g,b,a) de llum ambient de la font.
	vec4 diffuse;		// Intensitat (r,g,b,a) de llum difusa de la font.
	vec4 specular;		// Intensitat (r,g,b,a) de la llum especular de la font
	vec3 attenuation;	// Atenuaci� de la font per dist�ncia: .x (quadr�tica), .y (lineal), .z (constant).
	bool restricted;	// Booleana si font de llum restringida (TRUE) o no (FALSE).
	vec3 spotDirection;	// Vector de direcci� de la font restringida (Coord. M�n).
	float spotCosCutoff;
	float spotInnerCosCutoff;
	bool areaSource;
	vec3 areaAxisU;
	vec3 areaAxisV;
	vec2 areaHalfSize;
	float spotExponent;	// Exponent per al c�lcul intenstitat font restringida segons model Warn.
};

struct Material
{	vec4 emission;		// Coeficient d'emissi� (r,g,b,a) del material.
	vec4 ambient;		// Coeficient de reflectivitat ambient (r,g,b,a) del material.
        vec4 diffuse;		// Coeficient de reflectivitat difusa (r,g,b,a) del material.
	vec4 specular;		// Coeficient de reflectivitat especular (r,g,b,a) del material.
	float shininess;	// Exponent per al coeficient de reflectivitat especular del material (1,500).
};

// ---- L40- Variables in
in vec3 vertexPV;		// V�rtex en coordenades c�mera (Punt de Vista).
in vec3 vertexNormalPV;		// Normal en coordenades c�mera (Punt de Vista).
in vec2 vertexTexCoord;		// Coordenades textura.
in vec4 vertexColor;		// Color (r,g,b,a) del v�rtex.
in vec3 vertexWorldPosition;	// Posicion interpolada en coordenadas de mundo.

// ---- L46- Variables uniform
uniform mat4 normalMatrix;	// �the transpose of the inverse of the gl_ModelViewMatrix.� 
uniform mat4 projectionMatrix;	// Projection Matrix
uniform mat4 viewMatrix; 	// View Matrix
uniform mat4 modelMatrix;	// Model Matrix

uniform sampler2D texture0;	// Imatge textura
uniform bool textur;		// Booleana d�activaci� (TRUE) de textures o no (FALSE).
uniform bool flag_invert_y;	// Booleana que activa la inversi� coordenada textura t (o Y) a 1.0-cty segons llibreria SOIL (TRUE) o no (FALSE).
uniform bool modulate;		// Booleana d'activaci� de barreja color textura- color intensitat llum (TRUE) o nom�s color textura (FALSE)
uniform bool fixedLight;	// Booleana que defineix la font de llum fixe en Coordenades M�n (TRUE) o no (FALSE).
uniform bool sw_material;	// Booleana que indica si el color del v�rtex ve del Material emission, ambient, diffuse, specular (TRUE) o del vector de color del v�rtex in_Color (FALSE)
uniform bvec4 sw_intensity;	// Filtre per a cada tipus de reflexi�: Emissiva (sw_intensity[0]), Ambient (sw_intensity[1]), Difusa (sw_intensity[2]) o Especular (sw_intensity[2]).
uniform vec4 LightModelAmbient;	// Intensitat de llum ambient (r,g,b,a)-
uniform Light LightSource[MaxLights];	// Vector de fonts de llum.
uniform Material material;	// Vector de coeficients reflectivitat de materials.
uniform float uDirtStrength;
uniform float uIndirectIntensity;
uniform float uIndirectAmbientBase;
uniform float uSceneDirectScale;
uniform vec3 uIndirectFloorColor;
uniform vec3 uIndirectSkyColor;
uniform int uLightingDebugMode; // 0=final, 1=ambient, 2=directa, 3=indirecta, 4=normales, 5=albedo
uniform float uDirtScale;
uniform float uDirtSeed;
uniform float uTextureEmission;
uniform bool uMobileSurface;

// ---- L63- Variables out
out vec4 FragColor; 		// Color fragment (r,g,b,a)

bool segmentoBloqueadoPorArquitectura(vec3 origenMundo, vec3 destinoMundo)
{
	vec3 segmento = destinoMundo - origenMundo;
	float longitudSegmento = length(segmento);
	if (longitudSegmento < 1.0e-4) return false;
	float margenT = 0.01 / longitudSegmento;
 for (int indice = 0; indice < MaxOccluders; ++indice)
	{
		if (indice >= uOccluderCount) break;
		vec3 origen = (uOccluderWorldToLocal[indice] * vec4(origenMundo, 1.0)).xyz;
		vec3 destino = (uOccluderWorldToLocal[indice] * vec4(destinoMundo, 1.0)).xyz;
		vec3 direccion = destino - origen;
		float entrada = 0.0;
		float salida = 1.0;
		bool interseca = true;
		for (int eje = 0; eje < 3; ++eje)
		{
			if (abs(direccion[eje]) < 1.0e-7)
			{
				if (origen[eje] < -uOccluderHalfSize[indice][eje] || origen[eje] > uOccluderHalfSize[indice][eje])
					interseca = false;
			}
			else
			{
				float t0 = (-uOccluderHalfSize[indice][eje] - origen[eje]) / direccion[eje];
				float t1 = ( uOccluderHalfSize[indice][eje] - origen[eje]) / direccion[eje];
				entrada = max(entrada, min(t0, t1));
				salida = min(salida, max(t0, t1));
			}
		}
		// No auto-sombrear el emisor ni la cara receptora que coincide con la caja.
		if (interseca && entrada <= salida && salida > margenT && entrada > margenT && salida < 1.0 - margenT)
			return true;
	}
	return false;
}

bool segmentoBloqueadoParaRebote(vec3 origenMundo, vec3 destinoMundo)
{
	vec3 segmento = destinoMundo - origenMundo;
	float longitudSegmento = length(segmento);
	if (longitudSegmento < 1.0e-4) return false;
	float margenT = 0.01 / longitudSegmento;
	for (int indice = 0; indice < MaxIndirectOccluders; ++indice)
	{
		if (indice >= uIndirectOccluderCount) break;
		vec3 origen = (uIndirectOccluderWorldToLocal[indice] * vec4(origenMundo, 1.0)).xyz;
		vec3 destino = (uIndirectOccluderWorldToLocal[indice] * vec4(destinoMundo, 1.0)).xyz;
		vec3 direccion = destino - origen;
		float entrada = 0.0;
		float salida = 1.0;
		bool interseca = true;
		for (int eje = 0; eje < 3; ++eje)
		{
			if (abs(direccion[eje]) < 1.0e-7)
			{
				if (origen[eje] < -uIndirectOccluderHalfSize[indice][eje] ||
					origen[eje] > uIndirectOccluderHalfSize[indice][eje]) interseca = false;
			}
			else
			{
				float t0 = (-uIndirectOccluderHalfSize[indice][eje] - origen[eje]) / direccion[eje];
				float t1 = ( uIndirectOccluderHalfSize[indice][eje] - origen[eje]) / direccion[eje];
				entrada = max(entrada, min(t0, t1));
				salida = min(salida, max(t0, t1));
			}
		}
		if (interseca && entrada <= salida && salida > margenT && entrada > margenT && salida < 1.0 - margenT)
			return true;
	}
	return false;
}

vec3 phongModel(inout int numMat, inout float aValue)
{
// ---- L68- Calcul variables previes
    //mat4 NormalMatrix = transpose(inverse(viewMatrix * modelMatrix));
    vec3 N = normalize(vertexNormalPV);
    vec3 V = normalize(-vertexPV);
  
// ---- L73- Multiples llums
    float diffuseLight = 0.0;			// C�lcul del factor de reflexi� difusa depenent angles N,L.
    vec3 Idiffuse = vec3 (0.0,0.0,0.0);	        // Intensitat difusa d�una font de llum.
    vec3 R = vec3 (0.0,0.0,0.0);		// Vector R per a calcular la reflexio especular.
    float specularLight = 0.0;			// C�lcul del factor de reflexi� especular depenent angles R,V.
    vec3 Ispecular = vec3 (0.0,0.0,0.0);	// Intensitat especular d�una font de llum.
    vec3 ILlums = vec3 (0.0,0.0,0.0);	        // Intensitat acumulada per a totes les fonts de llum (r,g,b).
    float indirectLampAvailability = 0.0;
    vec4 lightPosition = vec4(0.0,0.0,0.0,1.0);	// Posici� de la font de llum en coordenades Punt de Vista.
    vec3 L = vec3 (0.0,0.0,0.0); 	        // Vector L per a c�lcul intensitat difusa i especular.
    float fatt=1.0;				// Factor d'atenuaci� llum per dist�ncia (fatt).
						// fatt = 1/Light.attenuation.x*d2 + Light.attenuation.y * d + Light.attenuation.z
    vec3 spotDirectionPV = vec3 (0.0,0.0,0.0);	// Vector spotDirection en coordenades c�mera i normalitzat.
    float spotDot; 				// Angle d'obertura en radians entre el v�rtex i el vector de la font restringida (spotDirection).

// ---- L87- Compute emissive term
    vec3 Iemissive = vec3 (0.0,0.0,0.0);	// Intensitat emissiva del fragment (pixel).
    if (sw_intensity[0])  {  if (sw_material) { Iemissive = material.emission.rgb * (uMaterialEmission ? uLampEmissionScale : 1.0);
                                                // C�lcul component a de transpar�ncia per llum emissiva.
				                numMat = numMat + 1;
					        aValue = material.emission.a;
                                              }
				else Iemissive = vertexColor.rgb;
			    Iemissive = clamp(Iemissive, 0.0, 1.0);
		          }
    
// ---- L98- Compute ambient term
    vec3 Iambient = vec3 (0.0,0.0,0.0);		// Intensitat ambient reflexada del fragment (pixel).
    if (sw_intensity[1]) { if (sw_material) { Iambient = material.ambient.rgb * LightModelAmbient.rgb;
                                              // C�lcul component a de transpar�ncia per llum emissiva.
				              numMat = numMat + 1;
					      aValue = material.ambient.a;
                                            }
				   else Iambient = vertexColor.rgb * LightModelAmbient.rgb;
			    Iambient = clamp(Iambient, 0.0, 1.0);
		         }

// ---- Luz directa con oclusión de cajas OBB y muestras rectangulares.
    mat4 inverseView = fixedLight ? mat4(1.0) : inverse(viewMatrix);
    vec3 normalWorld = normalize(mat3(inverseView) * N);
    float hemisphereUp = clamp(normalWorld.z * 0.5 + 0.5, 0.0, 1.0);
    float hemisphere = 0.30 + 0.70 * max(normalWorld.z, 0.0);
    vec3 baseDiffuse = sw_material ? material.diffuse.rgb : vertexColor.rgb;
    const vec2 areaPattern[4] = vec2[4](vec2(-0.5,-0.5), vec2(0.5,-0.5), vec2(-0.5,0.5), vec2(0.5,0.5));
    for (int i = 0; i < MaxLights; ++i)
    {
        if (!LightSource[i].sw_light) continue;
        int sampleCount = LightSource[i].areaSource ? 4 : 1;
        float sampleWeight = 1.0 / float(sampleCount);
        vec4 basePositionPV = fixedLight ? viewMatrix * LightSource[i].position : LightSource[i].position;
        vec3 basePositionWorld = fixedLight ? LightSource[i].position.xyz :
            (inverseView * vec4(LightSource[i].position.xyz, 1.0)).xyz;
        vec3 axisUPV = mat3(viewMatrix) * LightSource[i].areaAxisU;
        vec3 axisVPV = mat3(viewMatrix) * LightSource[i].areaAxisV;
        spotDirectionPV = normalize(mat3(viewMatrix) * LightSource[i].spotDirection);

        // Aproximacion de rebote por una sonda bajo cada luminaria. Su visibilidad
        // no depende del cono ni del rayo directo; la arquitectura y el radio limitan
        // la luz a la habitacion cercana.
        if (i >= 4 && LightSource[i].position.w == 1.0)
        {
            const float probeDrop = 0.75;
            const float probeRadius = 6.0;
            vec3 probePositionWorld = basePositionWorld - vec3(0.0, 0.0, probeDrop);
            vec3 probeToFragment = vertexWorldPosition - probePositionWorld;
            float probeDistance = length(probeToFragment);
            if (probeDistance < probeRadius && !segmentoBloqueadoParaRebote(probePositionWorld, vertexWorldPosition))
            {
                float distanceSquared = probeDistance * probeDistance;
                float attenuation = 1.0 / (1.0 + LightSource[i].attenuation.y * probeDistance +
                    LightSource[i].attenuation.x * distanceSquared);
                float roomFade = 1.0 - smoothstep(probeRadius * 0.68, probeRadius, probeDistance);
                float lampLuminance = dot(max(LightSource[i].diffuse.rgb, vec3(0.0)), vec3(0.2126, 0.7152, 0.0722));
                indirectLampAvailability += attenuation * roomFade * clamp(lampLuminance, 0.0, 1.0);
            }
        }

        for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex)
        {
            if (sampleIndex >= sampleCount) break;
            vec2 pattern = LightSource[i].areaSource ? areaPattern[sampleIndex] : vec2(0.0);
            vec3 offsetWorld = LightSource[i].areaSource ?
                LightSource[i].areaAxisU * pattern.x * LightSource[i].areaHalfSize.x +
                LightSource[i].areaAxisV * pattern.y * LightSource[i].areaHalfSize.y : vec3(0.0);
            vec4 samplePositionPV = basePositionPV + vec4(
                axisUPV * pattern.x * LightSource[i].areaHalfSize.x +
                axisVPV * pattern.y * LightSource[i].areaHalfSize.y, 0.0);
            vec3 samplePositionWorld = basePositionWorld + offsetWorld;
            float sampleFatt = 1.0;
            if (LightSource[i].position.w == 1.0)
            {
                vec3 lightToFragment = vertexPV - samplePositionPV.xyz;
                float distanceToLight = length(lightToFragment);
                sampleFatt = 1.0 / (LightSource[i].attenuation.x * distanceToLight * distanceToLight +
                    LightSource[i].attenuation.y * distanceToLight + LightSource[i].attenuation.z);
                L = normalize(-lightToFragment);
            }
            else L = normalize(-samplePositionPV.xyz);

            if (LightSource[i].restricted)
            {
                spotDot = dot(-L, spotDirectionPV);
                if (spotDot > LightSource[i].spotCosCutoff)
                {
                    spotDot = smoothstep(LightSource[i].spotCosCutoff, LightSource[i].spotInnerCosCutoff, spotDot);
                    sampleFatt *= pow(max(spotDot, 0.0), max(LightSource[i].spotExponent, 0.001));
                }
                else sampleFatt = 0.0;
            }
            if (sampleFatt > 0.0 && LightSource[i].position.w == 1.0 &&
                segmentoBloqueadoPorArquitectura(samplePositionWorld, vertexWorldPosition)) sampleFatt = 0.0;

            Idiffuse = vec3(0.0);
            if (sw_intensity[2])
            {
                diffuseLight = max(dot(L, N), 0.0);
                if (sw_material)
                {
                    Idiffuse = material.diffuse.rgb * LightSource[i].diffuse.rgb * diffuseLight * sampleFatt;
                    numMat += 1;
                    aValue += material.diffuse.a;
                }
                else Idiffuse = vertexColor.rgb * LightSource[i].diffuse.rgb * diffuseLight * sampleFatt;
                Idiffuse = clamp(Idiffuse, 0.0, 1.0);
            }

            Ispecular = vec3(0.0);
            if (sw_intensity[3])
            {
                R = normalize(-reflect(L, N));
                specularLight = pow(max(dot(R, V), 0.0), material.shininess);
                if (sw_material)
                {
                    Ispecular = material.specular.rgb * LightSource[i].specular.rgb * specularLight * sampleFatt;
                    numMat += 1;
                    aValue += material.specular.a;
                }
                else Ispecular = vertexColor.rgb * LightSource[i].specular.rgb * specularLight * sampleFatt;
                Ispecular = clamp(Ispecular, 0.0, 1.0);
            }
            ILlums += (Idiffuse + Ispecular) * sampleWeight * (i >= 4 ? clamp(uSceneDirectScale, 0.0, 1.5) : 1.0);
        }
    }
    vec3 emisionMTL = (uMaterialEmission && !(sw_material && sw_intensity[0]))
        ? material.emission.rgb * uLampEmissionScale : vec3(0.0);
    vec3 ambientHemisphereColor = mix(uIndirectFloorColor, uIndirectSkyColor, hemisphereUp);
    vec3 ambientBase = baseDiffuse * ambientHemisphereColor * clamp(uIndirectAmbientBase, 0.0, 0.15);
    vec3 indirect = baseDiffuse * uIndirectFloorColor * clamp(uIndirectIntensity, 0.0, 0.5) *
        clamp(indirectLampAvailability * 2.0, 0.0, 1.0) * hemisphere;
    if (uLightingDebugMode == 1) return Iambient + ambientBase;
    if (uLightingDebugMode == 2) return ILlums;
    if (uLightingDebugMode == 3) return indirect;
    if (uLightingDebugMode == 4) return normalWorld * 0.5 + 0.5;
   return Iemissive + emisionMTL + Iambient + ILlums + indirect + ambientBase;
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0)), f.x), f.y);
}

void main ()
{

// --- L173- Transpar�ncia
    int numMaterials = 0;		// Numero de materials actius a sw_material[].
    float aValor = 0.0;			// Valor de coordenada a de transpar�ncia acumulat dels materials. 
    vec4 fragmentColor = vec4(0.0,0.0,0.0,1.0);	// Intensitat final del fragment (pixel) segons equaci� il.luminaci�.

// ---- L178- Calcul intensitat final del pixel
   fragmentColor.rgb = phongModel(numMaterials, aValor);

// ---- L181- C�lcul component a de transpar�ncia.
   if (sw_material) { if (numMaterials != 0) fragmentColor.a = aValor / numMaterials; else fragmentColor.a = 1.0; // fragmentColor.a = material.diffuse.a;
                    }
	else fragmentColor.a = vertexColor.a;

   if (textur) { // Intensitat amb textura
		 vec4 colorT = texture(texture0,vertexTexCoord);
			 if (uMobileSurface) {
			     float detail = pow(clamp(max(max(colorT.r, colorT.g), colorT.b), 0.0, 1.0), 0.68);
			     float panel = smoothstep(0.025, 0.46, detail);
			     colorT.rgb = mix(vec3(0.065, 0.080, 0.090), vec3(0.25, 0.285, 0.30), panel);
			 }
			 if (uDirtStrength > 0.0) {
			     vec2 p = vertexWorldPosition.xy * max(uDirtScale, 0.05) + vec2(uDirtSeed, uDirtSeed * 0.618);
			     float broad = valueNoise(p * 0.72);
			     float fine = valueNoise(p * 2.1 + 19.7);
			     float stain = smoothstep(0.49, 0.79, broad * 0.78 + fine * 0.22);
			     float dust = smoothstep(0.60, 0.90, fine);
			     float dirt = clamp(stain * 0.78 + dust * 0.22, 0.0, 1.0);
			     colorT.rgb *= 1.0 - dirt * clamp(uDirtStrength, 0.0, 0.5);
			 }
		// Textura modulada amb intensitat llum
	    	if (modulate) FragColor = colorT * fragmentColor;
       			else FragColor = colorT; // textura sense modular intensitat llum
			 FragColor.rgb += vec3(clamp(uTextureEmission, 0.0, 0.35));
    	        }
    else { // Intensitat sense textura
           FragColor = fragmentColor;   
         }
    if (uLightingDebugMode == 5)
    {
        vec3 albedo = sw_material ? material.diffuse.rgb : vertexColor.rgb;
        if (textur) albedo *= texture(texture0, vertexTexCoord).rgb;
        FragColor = vec4(clamp(albedo, 0.0, 1.0), 1.0);
        return;
    }
    if (uLightingDebugMode > 0)
        FragColor = vec4(clamp(fragmentColor.rgb, 0.0, 1.0), fragmentColor.a);
}
