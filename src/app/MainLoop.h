#pragma once

struct Ps1Framebuffer;

// Runs frames until the window is asked to close. `previous` is the timer value of the first frame.
void RunMainLoop(const Ps1Framebuffer& ps1, float previous);
