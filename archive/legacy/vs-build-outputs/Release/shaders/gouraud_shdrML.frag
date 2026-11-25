//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (EE-UAB)
//******** Entorn bàsic VS2026 amb interfície MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// gouraud_shdrML.frag: Fragment Program en GLSL en versió OpenGL 3.3 o 4.60 per a implementar:
//     a) Iluminació de Gouraud
//     b) Fonts de llum puntuals o direccionals
//     c) Fonts de llum restringides
//     d) Atenuació de fonts de llum
//     e) Modus d'assignació de textura MODULATE o DECAL
//     f) Calcul component a de transparència segons color sigui provinent de material o de VAO
//     g) Algorisme incremental iluminació Gouraud encapsulat en gouraudModel()

//#version 330 core
#version 460 core

// --- L16- Variables in
in vec4 VertexColor;	// Color del Vèrtex
in vec2 VertexTexCoord;	// Coordenades textura del vèrtex

// --- L20- Variables uniform
uniform sampler2D texture0;	// Imatge textura
uniform bool textur;		// Booleana d’activació (TRUE) de textures o no (FALSE).
uniform bool modulate;		// Booleana d'activació de barreja color textura- color intensitat llum (TRUE) o només color textura (FALSE)

// --- L25- Variables out
out vec4 FragColor;		// Color fragment (r,g,b,a)

void main ()
{
if (textur) {	// Intensitat amb textura
		vec4 colorT = texture(texture0,VertexTexCoord);
		// Textura modulada amb intensitat llum
	    	if (modulate) FragColor = colorT * VertexColor;
       			else FragColor=colorT; // textura sense modular intensitat llum
    	    }
    else { // Intensitat sense textura
           FragColor = VertexColor;   
         }
}
