#pragma once

struct Ps1Framebuffer;

// Releases GPU, physics, network, VR and window resources while the GL context is still current.
void ShutdownApplication(Ps1Framebuffer& ps1);
