#pragma once

// Low resolution render target that gives PLAY mode its PS1 look. Needs a current OpenGL context.
struct Ps1Framebuffer
{
	static constexpr int kWidth = 960;
	static constexpr int kHeight = 540;

	unsigned int fbo = 0;
	unsigned int color = 0;
	unsigned int depth = 0;

	void Create();
	void Destroy();
};
