#pragma once

#include "src/app/AppState.h"

const scene::runtime::SceneRuntimeContext& contextoEscena();
void liberarObjetosAnimados();
bool cargarYAplicarEscena(const std::filesystem::path& ruta);
bool reconstruirFisicaTrasEdicion(std::string& error, bool autoridadFisica = true);
bool cargarModeloParaEditor(const std::filesystem::path& ruta, std::string& modeloCargado, std::string& error);
