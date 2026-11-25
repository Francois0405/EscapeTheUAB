//******** PRACTICA VISUALITZACIÓ GRÀFICA INTERACTIVA (EE-UAB)
//******** Entorn bàsic VS2026 amb interfície MFC/GLFW i Status Bar
//******** Enric Marti (Setembre 2026)
// skybox.frag: Fragment Program en GLSL en versió OpenGL 3.3 o 4.30 per a implementar Skybox

//version 330 core
#version 460 core

// ---- L9- Variables in, uniform i out
in vec3 TexCoords;
uniform samplerCube skybox;
out vec4 FragColor;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}