//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (EE-UAB)
//******** Entorn bàsic VS2026 amb interfície MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// eixos.frag: Fragment Program en GLSL en versió OpenGL 3.3 o 4.30 per a dibuixar eixos Coordenades Món

//#version 330 core
#version 460 core

// ---- L9- Variable in
in vec4 VertexColor;

// ---- L12- Variable in
out vec4 FragColor;

void main ()	// --- L15
{
// ---- L17- Intensitat sense textura
    FragColor = VertexColor; 
}
