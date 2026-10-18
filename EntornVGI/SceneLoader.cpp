#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <Windows.h>

// Lee un archivo de escena y comprueba sus objetos.
// Cada entrada contiene el modelo y sus transformaciones.
void ComprobarArchivoEscena(const std::filesystem::path& ruta)
{
    std::ifstream archivo(ruta);

    if (!archivo.is_open())
    {
        OutputDebugStringA("No se ha podido abrir el archivo de escena.\n");
        return;
    }

    OutputDebugStringA("Archivo de escena abierto correctamente.\n");

    // Pendiente:
    // 1. Leer las líneas del archivo.
    // 2. Ignorar las líneas vacías y los comentarios.
    // 3. Obtener el modelo y sus nueve valores de transformación.
    // 4. Comprobar que los datos son válidos.
    // 5. Guardar los objetos para utilizarlos en la escena.

    // Leer las entradas del archivo
    std::string linea;
    int numObjetos = 0;

    while (std::getline(archivo, linea))
    {
        // Empezamos con la parte de importacion de los obj a dentro del entorno

        struct SceneObject
        {
            std::string modelo;

            glm::vec3 posicion;
            glm::vec3 rotacion;
            glm::vec3 escala;
        };

        // Posición del objeto.
        float x, y, z;

        // Rotación en grados.
        float rx, ry, rz;

        // Escala del objeto.
        float sx, sy, sz;

        std::istringstream datos(linea);
        std::string modelo;

        // Saltar líneas vacías y comentarios.
        if (!(datos >> modelo) || modelo[0] == '#')
            continue;

        if (!(datos >> x >> y >> z
            >> rx >> ry >> rz
            >> sx >> sy >> sz))
        {
            OutputDebugStringA("Error: faltan datos o hay valores incorrectos.\n");
            continue;
        }


        numObjetos++;

        // Mostrar el modelo detectado.
        std::ostringstream mensaje;

        mensaje << "Modelo: " << modelo
            << " | Posicion: " << x << ", " << y << ", " << z
            << " | Rotacion: " << rx << ", " << ry << ", " << rz
            << " | Escala: " << sx << ", " << sy << ", " << sz
            << "\n";

        OutputDebugStringA(mensaje.str().c_str());
    }

    std::string resumen = "Objetos detectados: "
        + std::to_string(numObjetos) + "\n";

    OutputDebugStringA(resumen.c_str());
}