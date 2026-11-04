// platform_compat.h: pequeñas adaptaciones para compilar el proyecto en
// Linux/GCC manteniendo intacta la compilación original con Visual Studio.
//
// En Windows este archivo se limita a incluir <windows.h>; en el resto de
// plataformas proporciona equivalentes de las funciones "seguras" de MSVC y
// de los tipos/macros de Windows que usan algunos archivos del proyecto.

#pragma once

#ifdef _WIN32

#include <windows.h>

#else // ---------------------------------------------------------------------

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// Convenciones de llamada y atributos específicos de MSVC.
#ifndef _stdcall
#define _stdcall
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

// Tipos usados por la cabecera del cargador OBJ.
typedef unsigned int uint;
typedef int BOOL;

// fopen_s de MSVC: devuelve 0 si todo va bien o el código de error.
inline int fopen_s(FILE** pFile, const char* filename, const char* mode)
{
	if (pFile == nullptr)
		return EINVAL;

	errno = 0;
	*pFile = std::fopen(filename, mode);
	if (*pFile != nullptr)
		return 0;

	return errno != 0 ? errno : EIO;
}

// Versiones seguras de strcpy/strcat limitadas al tamaño del array destino.
template <std::size_t N>
inline int strcpy_s(char (&dest)[N], const char* src)
{
	if (src == nullptr)
	{
		if (N > 0) dest[0] = '\0';
		return EINVAL;
	}
	if (std::strlen(src) >= N)
	{
		if (N > 0) dest[0] = '\0';
		return ERANGE;
	}
	std::strcpy(dest, src);
	return 0;
}

template <std::size_t N>
inline int strcat_s(char (&dest)[N], const char* src)
{
	if (src == nullptr)
		return EINVAL;

	const std::size_t destLen = std::strlen(dest);
	const std::size_t srcLen = std::strlen(src);
	if (destLen + srcLen >= N)
	{
		if (N > 0) dest[0] = '\0';
		return ERANGE;
	}
	std::strcat(dest, src);
	return 0;
}

// fscanf_s/sscanf_s sin comprobación adicional de buffers: el proyecto solo
// los usa con destinos escalares (%f, %i), igual que fscanf/sscanf.
inline int fscanf_s(FILE* stream, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = std::vfscanf(stream, format, args);
	va_end(args);
	return result;
}

inline int sscanf_s(const char* buffer, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	const int result = std::vsscanf(buffer, format, args);
	va_end(args);
	return result;
}

// Salida de depuración equivalente a la API de Windows.
inline void OutputDebugStringA(const char* text)
{
	if (text != nullptr)
		std::fputs(text, stderr);
}

#endif // _WIN32
