//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (EE-UAB)
//******** Entorn bàsic VS2026 amb interficie MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// gouraud_shdrML.vert: Vertex Program en GLSL en versió OpenGL 3.3 o 4.60 per a implementar:
//     a) Iluminació de Gouraud
//     b) Fonts de llum puntuals o direccionals
//     c) Fonts de llum restringides
//     d) Atenuació de fonts de llum
//     e) Modus d'assignació de textura MODULATE o DECAL
//     f) Calcul component a de transparència segons color sigui provinent de material o de VAO
//     g) Algorisme incremental  iluminació Gouraud encapsulat en gouraudModel()

//#version 330 core
#version 460 core

#define MaxLights 8

// --- L16- Estructures Font de LLum i Material (coeficients reflectivitat).
struct Light
{	bool sw_light;		// Booleana que indica si llum encesa (TRUE) o apagada (FALSE).
	vec4 position;		// Posició de la font de llum, en Coord. Món o Càmera (Punt de Vista).
	vec4 ambient;		// Intensitat (r,g,b,a) de llum ambient de la font.
	vec4 diffuse;		// Intensitat (r,g,b,a) de llum difusa de la font.
	vec4 specular;		// Intensitat (r,g,b,a) de la llum especular de la font
	vec3 attenuation;	// Atenuació de la font per distància: .x (quadràtica), .y (lineal), .z (constant).
	bool restricted;	// Booleana si font de llum restringida (TRUE) o no (FALSE).
	vec3 spotDirection;	// Vector de direcció de la font restringida (Coord. Món).
	float spotCosCutoff;	// Angle d'obertura de la font restringids (en graus). 
	float spotExponent;	// Exponent per al càlcul intenstitat font restringida segons model Warn.
};

struct Material
{	vec4 emission;		// Coeficient d'emissió (r,g,b,a) del material.
	vec4 ambient;		// Coeficient de reflectivitat ambient (r,g,b,a) del material.
        vec4 diffuse;		// Coeficient de reflectivitat difusa (r,g,b,a) del material.
	vec4 specular;		// Coeficient de reflectivitat especular (r,g,b,a) del material.
	float shininess;	// Exponent per al coeficient de reflectivitat especular del material (1,500).
};

// --- L40- Variables in
layout (location = 0) in vec3 in_Vertex; 	// Coordenades (x,y,z) posicio Vertex.
layout (location = 1) in vec4 in_Color; 	// Coordenades (r,g,b,a) Color.
layout (location = 2) in vec3 in_Normal; 	// Coordenades (x,y,z) Vector Normal.
layout (location = 3) in vec2 in_TexCoord; 	// Coordenades (s,t) Coordenada textura.

// --- L46- Variables uniform
//uniform mat4 normalMatrix;	// “the transpose of the inverse of the ModelViewMatrix.” 
uniform mat4 projectionMatrix;	// Projection Matrix.
uniform mat4 viewMatrix; 	// View Matrix.
uniform mat4 modelMatrix;	// Model Matrix.

uniform sampler2D texture0;	// Imatge textura
uniform bool textur;		// Booleana d’activació (TRUE) de textures o no (FALSE).
uniform bool flag_invert_y;	// Booleana que activa la inversió coordenada textura t (o Y) a 1.0-cty segons llibreria SOIL (TRUE) o no (FALSE).
uniform bool fixedLight;	// Booleana que defineix la font de llum fixe en Coordenades Món (TRUE) o no (FALSE).
uniform bool sw_material;	// Booleana que indica si el color del vèrtex ve del Material emission, ambient, diffue, specular (TRUE) o del vector de color del vèrtex in_Color (FALSE)
uniform bvec4 sw_intensity;	// Filtre per a cada tipus de reflexió: Emissiva (sw_intensity[0]), Ambient (sw_intensity[1]), Difusa (sw_intensity[2]) o Especular (sw_intensity[2]).
uniform vec4 LightModelAmbient;	// Intensitat de llum ambient (r,g,b,a)-
uniform Light LightSource[MaxLights];	// Vector de fonts de llum.
uniform Material material;	// Vector de coeficients reflectivitat de materials.

// --- L62- Variables out
out vec4 VertexColor;
out vec2 VertexTexCoord;

vec3 gouraudModel(inout int numMat, inout float aValue)
{
// --- L68- Calcul Vector Normal
    mat4 normalMatrix = transpose(inverse(viewMatrix * modelMatrix));
    vec3 N = vec3(normalMatrix * vec4(in_Normal,1.0));
    N = normalize(N);
    vec3 vertexPV = vec3(viewMatrix * modelMatrix * vec4(in_Vertex,1.0));
    vec3 V = normalize(-vertexPV);

// --- L75- Multiples llums
    vec3 ILlums = vec3 (0.0,0.0,0.0);		// Intensitat acumulada per a totes les fonts de llum.
    vec3 Idiffuse = vec3 (0.0,0.0,0.0);		// Intensitat difusa d’una font de llum.
    vec3 Ispecular = vec3 (0.0,0.0,0.0);	// Intensitat especular d’una font de llum.
    vec4 lightPosition = vec4(0.0,0.0,0.0,1.0);	// Posició de la font de llum en coordenades Punt de Vista.
    vec3 L = vec3 (0.0,0.0,0.0);		// Vector L per a càlcul intensitat difusa i especular.
    float fatt=1.0;				// Factor d'atenuació llum per distància (fatt).
						// fatt = 1/Light.attenuation.x*d2 + Light.attenuation.y * d + Light.attenuation.z
    vec3 spotDirectionPV = vec3 (0.0,0.0,0.0);	// Vector spotDirection en coordenades càmera i normalitzat.
    float spotDot; 				// Angle d'obertura en radians entre el vèrtex i el vector de la font restringida (spotDirection).

// --- L86- Transparència
    //int numMat = 0;				// Numero de materials actius a sw_material[].
    //float aValue = 0.0;			// Valor de coordenada a de transparència acumulat. 

// --- L90- Textura
        //VertexTexCoord = in_TexCoord;
	if (flag_invert_y) VertexTexCoord = vec2(in_TexCoord.x,1.0-in_TexCoord.y); // SOIL_FLAG_INVERT_Y
 	 else VertexTexCoord = vec2(in_TexCoord.x,in_TexCoord.y);

// --- L95- Compute emissive term
    vec3 Iemissive = vec3 (0.0,0.0,0.0);	// Intensitat emissiva de l’objecte.
    if (sw_intensity[0])  {	if (sw_material) { Iemissive = material.emission.rgb;
                                                   // Càlcul component a de transparència per llum emissiva.
						   numMat = numMat + 1;
						   aValue = material.emission.a;
						 }
					else Iemissive = in_Color.rgb;
			  }

// --- L105- Compute ambient term
    vec3 Iambient = vec3 (0.0,0.0,0.0);		// Intensitat ambient reflexada
    if (sw_intensity[1]) {	if (sw_material) { Iambient = material.ambient.rgb * LightModelAmbient.rgb;
                                                   // Càlcul component a de transparència per llum ambient i per material.
					           numMat = numMat + 1;
					           aValue = aValue + material.ambient.a;
                                                 }
					else Iambient = in_Color.rgb * LightModelAmbient.rgb;
        			Iambient = clamp(Iambient, 0.0, 1.0);
    			}

// --- L116- Multiples llums
    for (int i=0;i<MaxLights;i++) {
	if (LightSource[i].sw_light) {
		fatt = 1.0; 	// Inicialitzar factor d'atenuació. 
		// --- L120- Compute light position (fixed in WC of attached to camera)
		if (fixedLight) lightPosition = viewMatrix * LightSource[i].position;
			else lightPosition = vec4(-vertexPV,1.0);

		// --- L124- Compute point light source (w=1) or direccional light (w=0)
		if (LightSource[i].position.w == 1) 
		  { //L = normalize(lightPosition.xyz - vertexPV);
		    L = lightPosition.xyz - vertexPV;
  		    // --- L128- Compute Attenuation factor
		    fatt = length(L);		// Càlcul de la distància entre la posició de la font de llum i el vèrtex.
		    fatt = 1 / (LightSource[i].attenuation.x * fatt * fatt + LightSource[i].attenuation.y * fatt + LightSource[i].attenuation.z);
		    L = normalize(L);		// Normalitzar el vector.
	          }
                     else L = normalize(-lightPosition.xyz);

		// --- L135- Compute Spot Light Factor
		if (LightSource[i].restricted) 
                  { spotDirectionPV = vec3(normalMatrix * vec4(LightSource[i].spotDirection,1.0));
  		    spotDirectionPV = normalize(spotDirectionPV);
		    spotDot = dot(-L,spotDirectionPV);
		    if (spotDot > LightSource[i].spotCosCutoff)
		      { spotDot = pow(spotDot, LightSource[i].spotExponent); // Model de Warn
			fatt = spotDot * fatt;
                      }
		      else fatt=0.0;
                  }

		// --- L147- Compute the diffuse term
		Idiffuse = vec3 (0.0,0.0,0.0);
     		if (sw_intensity[2]) {
        		float diffuseLight = max(dot(L, N), 0.0);
			if (sw_material) { Idiffuse = material.diffuse.rgb * LightSource[i].diffuse.rgb * diffuseLight * fatt;
				           // Càlcul component a de transparència per llum difusa i per material.
					   numMat = numMat +1;
					   aValue = aValue + material.diffuse.a;
                                         }
                                else Idiffuse = in_Color.rgb * LightSource[i].diffuse.rgb * diffuseLight * fatt;
        		Idiffuse = clamp(Idiffuse, 0.0, 1.0);
     			}

		// --- L160- Compute the specular term
    		Ispecular = vec3 (0.0,0.0,0.0);
    		if (sw_intensity[3]) {
        		//vec3 V = normalize(-vertexPV);
        		vec3 R = normalize(-reflect(L,N));
        		float specularLight = pow(max(dot(R, V), 0.0), material.shininess);
			if (sw_material) { Ispecular = material.specular.rgb * LightSource[i].specular.rgb * specularLight * fatt;
				           // Càlcul component a de transparència per llum difusa i per material.
					   numMat = numMat + 1;
					   aValue = aValue + material.specular.a;
                                         }
				else Ispecular = in_Color.rgb * LightSource[i].specular.rgb * specularLight * fatt;
        		Ispecular = clamp(Ispecular, 0.0, 1.0);
    			}

   		ILlums += Idiffuse + Ispecular;
		}
   }
    
    return Iemissive + Iambient + ILlums;
}

void main ()	// --- L182-
{
// --- L184- Calcul Vector Normal
    
// --- L186- Transformacio de Visualitzacio amb Matriu Projeccio (projectionMatrix), Matriu Càmera (viewMatrix) i Matriu TG (modelMatrix)
    gl_Position = vec4(projectionMatrix * viewMatrix * modelMatrix * vec4(in_Vertex,1.0));

// --- L189- Transparència
    int numMaterials = 0;		// Numero de materials actius a sw_material[].
    float aValor = 0.0;			// Valor de coordenada a de transparència acumulat.

// --- L193- Calcul intensitat final del vertex segons algorisme incremental de Gouraud
    VertexColor.rgb = gouraudModel(numMaterials,aValor);

// --- L196- Càlcul component a de transparència.
    // if (sw_material) vertexColor.a = material.diffuse.a; else VertexColor.a = in_Color.a;
    if (sw_material) { if (numMaterials != 0) VertexColor.a = aValor / numMaterials; else VertexColor.a = 1.0;
                    }
	else VertexColor.a = in_Color.a;
}