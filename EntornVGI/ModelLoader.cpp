#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <string>
#include <iostream>
#include <Windows.h>

void DoTheSceneProcessing(const aiScene* scene)
{

    if (scene == nullptr)
    {
        OutputDebugStringA("Error: escena nula\n");
        return;
    }

    std::string mensaje = "Mallas importadas: " + std::to_string(scene->mNumMeshes) + "\n";
    OutputDebugStringA(mensaje.c_str());

}

bool DoTheImportThing(const std::string& pFile) {
    // Create an instance of the Importer class
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile(pFile,
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    // If the import failed, report it
    if (nullptr == scene) {
        // -----------------CODIGO NUESTRO------------------
        OutputDebugStringA(importer.GetErrorString());
        OutputDebugStringA("\n");
        return false;
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];

        // Consultamos el tamaño de cada malla importada.
        std::string mensaje = "Malla " + std::to_string(i)
            + ": " + std::to_string(mesh->mNumVertices) + " vertices, "
            + std::to_string(mesh->mNumFaces) + " caras\n";

        OutputDebugStringA(mensaje.c_str());
    }

    // -----------------CODIGO NUESTRO------------------
    DoTheSceneProcessing(scene);

    // We're done. Everything will be cleaned up by the importer destructor
    return true;
}