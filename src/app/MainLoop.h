#pragma once

struct Ps1Framebuffer;
class RetroPostFX;

// Runs frames until the window is asked to close. `previous` is the timer value of the first frame.
void RunMainLoop(Ps1Framebuffer& ps1, float previous, const RetroPostFX& postFx);
