#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <vector>
#include <Windows.h>
#include "SceneLoader.h"

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

    // Leemos la ruta y las transformaciones de cada objeto.
    std::string linea;

    std::vector<SceneObject> objetos;

    while (std::getline(archivo, linea))
    {
        // Empezamos con la parte de importacion de los obj a dentro del entorno

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

        // Guardamos el modelo y sus transformaciones.
        objetos.push_back({
            modelo,
            {x, y, z},
            {rx, ry, rz},
            {sx, sy, sz}
            });

        // Mostrar el modelo detectado.
        std::ostringstream mensaje;

        mensaje << "Modelo: " << modelo
            << " | Posicion: " << x << ", " << y << ", " << z
            << " | Rotacion: " << rx << ", " << ry << ", " << rz
            << " | Escala: " << sx << ", " << sy << ", " << sz
            << "\n";

        OutputDebugStringA(mensaje.str().c_str());
    }
}