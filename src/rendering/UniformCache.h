#pragma once

#include <GL/glew.h>
#include <string>
#include <unordered_map>

// Uniform locations belong to a linked program. A new link must invalidate the
// entry before that program name is reused.
namespace glcache
{
	inline bool enabled = true;
	inline std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> locations;
	inline GLint UniformLocation(GLuint program, const char* name)
	{
		if (!enabled) return glGetUniformLocation(program, name);
		auto& perProgram = locations[program];
		auto it = perProgram.find(name);
		if (it != perProgram.end()) return it->second;
		return perProgram.emplace(name, glGetUniformLocation(program, name)).first->second;
	}
	inline void InvalidateProgram(GLuint program) { locations.erase(program); }
	inline void InvalidateAll() { locations.clear(); }
}
