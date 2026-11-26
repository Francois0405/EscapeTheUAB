//******** PRACTICA VISUALITZACI� GR�FICA INTERACTIVA (EE-UAB)
//******** Entorn b�sic VS2026 amb interf�cie MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// phong_shdrML.vert: Vertex Program en GLSL en versi� OpenGL 3.3 o 4.60 per a implementar:
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

// ---- L18- Estructures Font de LLum i Material (coeficients reflectivitat).
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

// --- L40- Variables in
layout (location = 0) in vec3 in_Vertex; 	// Coordenades (x,y,z) posicio Vertex
layout (location = 1) in vec4 in_Color; 	// Coordenades (r,g,b,a) Color
layout (location = 2) in vec3 in_Normal; 	// Coordenades (x,y,z) Vector Normal
layout (location = 3) in vec2 in_TexCoord; 	// Coordenades (s,t) Coordenada textura

// --- L46- Variables uniform
uniform mat4 normalMatrix;	// �the transpose of the inverse of the gl_ModelViewMatrix.� 
uniform mat4 projectionMatrix;	// Projection Matrix
uniform mat4 viewMatrix; 	// View Matrix
uniform mat4 modelMatrix;	// Model Matrix

uniform sampler2D texture0;	// Imatge textura
uniform bool textur;		// Booleana d�activaci� (TRUE) de textures o no (FALSE).
uniform bool flag_invert_y;	// Booleana que activa la inversi� coordenada textura t (o Y) a 1.0-cty segons llibreria SOIL (TRUE) o no (FALSE).
// Escala fisica de la textura para los cubos arquitectonicos.
uniform bool uWorldTiling;
uniform vec2 uWorldTileSize;
uniform bool fixedLight;	// Booleana que defineix la font de llum fixe en Coordenades M�n (TRUE) o no (FALSE).
uniform bool sw_material;	// Booleana que indica si el color del v�rtex ve del Material emission, ambient, diffuse, specular (TRUE) o del vector de color del v�rtex in_Color (FALSE)
uniform bvec4 sw_intensity;	// Filtre per a cada tipus de reflexi�: Emissiva (sw_intensity[0]), Ambient (sw_intensity[1]), Difusa (sw_intensity[2]) o Especular (sw_intensity[2]).
uniform vec4 LightModelAmbient;	// Intensitat de llum ambient (r,g,b,a).
uniform Light LightSource[MaxLights];	// Vector de fonts de llum.
uniform Material material;	// Vector de coeficients reflectivitat de materials.

// --- L62- Variables out
out vec3 vertexPV;
out vec3 vertexNormalPV;
out vec2 vertexTexCoord;
out vec4 vertexColor;
out vec3 vertexWorldPosition;

// Cordura baja: deformacion de la geometria (ondas, franjas desplazadas a saltos y cuantizacion de vertices).
// Es funcion de la posicion en el mundo, asi que las costuras entre mallas se mueven juntas. 0 = sin efecto.
uniform float uInsanity;
uniform float uInsanityTime;

float insHash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }

vec3 insanityWorld(vec3 wp)
{
    float s = smoothstep(0.12, 1.0, uInsanity);
    if (s <= 0.0) return wp;
    float t = uInsanityTime;
    vec3 w = vec3(sin(wp.z * 2.1 + t * 1.6 + wp.y * 1.3),
                  sin(wp.x * 1.7 - t * 2.1 + wp.z * 1.1),
                  sin(wp.y * 2.3 + t * 1.9 + wp.x * 0.9));
    vec3 off = w * (0.015 + 0.14 * s) * s;
    float paso = floor(t * 7.0);
    float franja = floor(wp.z * 3.0);
    float burst = step(1.0 - 0.45 * s * s, insHash(vec3(franja, paso, 1.0)));
    off.x += burst * (insHash(vec3(franja, paso, 2.0)) - 0.5) * 1.2 * s;
    off.y += burst * (insHash(vec3(franja, paso, 3.0)) - 0.5) * 1.2 * s;
    wp += off;
    float g = mix(0.0, 0.35, s * s * s);
    if (g > 0.01) wp = mix(wp, floor(wp / g + 0.5) * g, smoothstep(0.35, 0.9, s));
    return wp;
}

void main()	// --- L68-
{
// --- L70- Calcul variables out.
    vec3 insanityPos = insanityWorld(vec3(modelMatrix * vec4(in_Vertex, 1.0)));
    vertexPV = vec3(viewMatrix * vec4(insanityPos, 1.0));
    vertexWorldPosition = vec3(modelMatrix * vec4(in_Vertex, 1.0));

    //mat4 NormalMatrix = transpose(inverse(viewMatrix * modelMatrix));
    mat3 normalMatrixLocal =
        transpose(inverse(mat3(viewMatrix * modelMatrix)));

    vertexNormalPV = normalize(normalMatrixLocal * in_Normal);

// --- L77- Textura
// Proyeccion planar en coordenadas mundo: conserva metros por baldosa
    // aunque se escale o rote la misma geometria para otra habitacion.
    if (uWorldTiling) {
        vec3 p = vec3(modelMatrix * vec4(in_Vertex, 1.0));
        vec3 n = abs(normalize(mat3(modelMatrix) * in_Normal));
        vec2 tile = max(uWorldTileSize, vec2(0.1));
        if (n.z >= n.x && n.z >= n.y) vertexTexCoord = p.xy / tile;
        else if (n.x >= n.y) vertexTexCoord = p.yz / tile;
        else vertexTexCoord = p.xz / tile;
    } else {
        vertexTexCoord = flag_invert_y
            ? vec2(in_TexCoord.x, 1.0 - in_TexCoord.y)
            : in_TexCoord;
    }
    
// --- L81- Pas color del vertex al Fragent Shader
    vertexColor = in_Color;

// --- L84- Transformacio de Visualitzacio amb Matriu Projeccio (PMatrix), Matriu Cmera (VMatrix) i Matriu TG (MMatrix)
    gl_Position = vec4(projectionMatrix * viewMatrix * vec4(insanityPos, 1.0));
}
