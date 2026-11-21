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

#define MaxLights 8
uniform bool uMaterialEmission;

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

// ---- L63- Variables out
out vec4 FragColor; 		// Color fragment (r,g,b,a)

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
    vec4 lightPosition = vec4(0.0,0.0,0.0,1.0);	// Posici� de la font de llum en coordenades Punt de Vista.
    vec3 L = vec3 (0.0,0.0,0.0); 	        // Vector L per a c�lcul intensitat difusa i especular.
    float fatt=1.0;				// Factor d'atenuaci� llum per dist�ncia (fatt).
						// fatt = 1/Light.attenuation.x*d2 + Light.attenuation.y * d + Light.attenuation.z
    vec3 spotDirectionPV = vec3 (0.0,0.0,0.0);	// Vector spotDirection en coordenades c�mera i normalitzat.
    float spotDot; 				// Angle d'obertura en radians entre el v�rtex i el vector de la font restringida (spotDirection).

// ---- L87- Compute emissive term
    vec3 Iemissive = vec3 (0.0,0.0,0.0);	// Intensitat emissiva del fragment (pixel).
    if (sw_intensity[0])  {  if (sw_material) { Iemissive = material.emission.rgb;
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

// ---- L109- Multiples llums
    for (int i=0;i<MaxLights;i++) {
	if (LightSource[i].sw_light) { 
		fatt = 1.0; 	// Inicialitzar factor d'atenuaci�.
		// ---- L113- Compute light position (fixed in WC of attached to camera)
		if (fixedLight) lightPosition = viewMatrix * LightSource[i].position;
		  else lightPosition = LightSource[i].position; // vec4(-vertexPV,1.0);
		
		// ---- L117- Compute point light source (w=1) or direccional light (w=0)
		if (LightSource[i].position.w == 1) 
		  {  //L = normalize(lightPosition.xyz - vertex-PV);
		     L = lightPosition.xyz - vertexPV;
		     // -L122- Compute Attenuation factor
		     fatt = length(L);		// C�lcul de la dist�ncia entre la posici� de la font de llum i el v�rtex.
		     fatt = 1 / (LightSource[i].attenuation.x * fatt * fatt + LightSource[i].attenuation.y * fatt + LightSource[i].attenuation.z);
		     L = normalize(L);		// Normalitzar el vector.
		  }
                   else L = normalize(-lightPosition.xyz);  //else L = normalize(lightPosition.xyz);

		// ---- L128- Compute Spot Light Factor
		if (LightSource[i].restricted) 
                  {
			spotDirectionPV = normalize(
				mat3(viewMatrix) * LightSource[i].spotDirection
			);
		    spotDot = dot(-L,spotDirectionPV);
		    if (spotDot > LightSource[i].spotCosCutoff)
		      {
			spotDot = smoothstep(LightSource[i].spotCosCutoff,
				LightSource[i].spotInnerCosCutoff, spotDot);
			spotDot = pow(max(spotDot, 0.0), max(LightSource[i].spotExponent, 0.001));
			fatt = spotDot * fatt;
	                      }
		      else fatt=0.0;
                  }

		// ---- L140- Compute the diffuse term
    		Idiffuse = vec3 (0.0,0.0,0.0);	// Intensitat difusa de la font de llum i-�ssima reflexada del fragment (pixel).
     		if (sw_intensity[2]) {	diffuseLight = max(dot(L, N), 0.0);
    					if (sw_material) { Idiffuse = material.diffuse.rgb * LightSource[i].diffuse.rgb * diffuseLight * fatt;
                                                           // C�lcul component a de transpar�ncia per llum difusa i per material.
					                   numMat = numMat +1;
					                   aValue = aValue + material.diffuse.a;
                                                         }
					      else Idiffuse = vertexColor.rgb * LightSource[i].diffuse.rgb * diffuseLight * fatt;
    					Idiffuse = clamp(Idiffuse, 0.0, 1.0);
    				     }

		// ---- 152- Compute the specular term
    		Ispecular = vec3 (0.0,0.0,0.0);	// Intensitat especular de la font de llum i-�ssima reflexada del fragment (pixel).
    		if (sw_intensity[3]) {	R = normalize (-reflect(L,N));
    					specularLight = pow(max(dot(R, V), 0.0), material.shininess);
    					if (sw_material) { Ispecular = material.specular.rgb * LightSource[i].specular.rgb * specularLight * fatt;
                                                           // C�lcul component a de transpar�ncia per llum especular per material.
					                   numMat = numMat +1;
					                   aValue = aValue + material.specular.a;
                                                         }
					      else Ispecular = vertexColor.rgb * LightSource[i].specular.rgb * specularLight * fatt; 
    					Ispecular = clamp(Ispecular, 0.0, 1.0);
				     }
		ILlums += Idiffuse + Ispecular;
		}
   }
   vec3 emisionMTL = (uMaterialEmission && !sw_intensity[0]) ? material.emission.rgb : vec3(0.0);
   return Iemissive + emisionMTL + Iambient + ILlums;
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
		// Textura modulada amb intensitat llum
	    	if (modulate) FragColor = colorT * fragmentColor;
       			else FragColor = colorT; // textura sense modular intensitat llum
    	        }
    else { // Intensitat sense textura
           FragColor = fragmentColor;   
         }
}
