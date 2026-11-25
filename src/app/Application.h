#pragma once

// Application lifecycle, in the order main() runs it.
void ParseNetworkArguments(int argc, char** argv);
bool CreateMainWindow();            // GLFW window, GLEW, native dialogs and GL debug output
void InitializeGraphicsResources(); // legacy VGI GL state and shared models
void RegisterWindowCallbacks();     // GLFW callbacks and editor asset loaders
void InitializeImGui();
bool StartGame();                   // gameplay preset, startup scene, player and optional network
void LoadMenuBackground();
