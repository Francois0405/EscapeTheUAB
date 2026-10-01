//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (EE-UAB)
//******** Entorn bàsic VS2026 amb interfície MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// eixos.vert: Vertex Program en GLSL en versió OpenGL 3.3 o 4.30 per a dibuixar eixos Coordenades Món

//#version 330 core
#version 460 core

// --- L9- Variables in
layout (location = 0) in vec3 in_Vertex; 	// Coordenades (x,y,z) posicio Vertex
layout (location = 1) in vec4 in_Color; 	// Coordenades (r,g,b,a) Color

// --- L13- Variables uniform
uniform mat4 projectionMatrix;	// Projection Matrix
uniform mat4 viewMatrix; 	// View Matrix

// --- L17- Variables out
out vec4 VertexColor;

void main ()	// --- L20
{
// --- L22- Transformacio de Visualitzacio amb Matriu Projeccio (projectionMatrix), Matriu Càmera (viewMatrix) i Matriu TG (modelMatrix)
    gl_Position = vec4(projectionMatrix * viewMatrix * vec4(in_Vertex,1.0));

// ---L25- Calcul intensitat final del vertex
    VertexColor = in_Color;
}