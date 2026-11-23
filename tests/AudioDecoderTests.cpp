#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main()
{
	const std::filesystem::path audioDirectory =
		std::filesystem::path(ENTORNVGI_SOURCE_ASSETS_DIR) / "audio" / "hirity";
	int fallos = 0;
	int archivosProbados = 0;

	for (const auto& entrada : std::filesystem::recursive_directory_iterator(audioDirectory))
	{
		if (!entrada.is_regular_file() || entrada.path().extension() != ".ogg") continue;
		++archivosProbados;

		ma_decoder decoder{};
		const std::string ruta = entrada.path().string();
		ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
		ma_result resultado = ma_decoder_init_file(ruta.c_str(), &config, &decoder);
		if (resultado != MA_SUCCESS)
		{
			std::cerr << "FALLO: no se pudo abrir " << ruta << " (codigo " << resultado << ").\n";
			++fallos;
			continue;
		}

		ma_format formato;
		ma_uint32 canales = 0, frecuencia = 0;
		resultado = ma_decoder_get_data_format(&decoder, &formato, &canales, &frecuencia, nullptr, 0);
		if (resultado != MA_SUCCESS || formato != ma_format_f32 || canales == 0 || frecuencia == 0)
		{
			std::cerr << "FALLO: formato de audio invalido en " << ruta << ".\n";
			++fallos;
			ma_decoder_uninit(&decoder);
			continue;
		}

		std::vector<float> muestras(static_cast<std::size_t>(512) * canales);
		ma_uint64 fotogramasLeidos = 0;
		resultado = ma_decoder_read_pcm_frames(&decoder, muestras.data(), 512, &fotogramasLeidos);
		if (resultado != MA_SUCCESS || fotogramasLeidos == 0)
		{
			std::cerr << "FALLO: no se pudieron decodificar muestras de " << ruta
				<< " (codigo " << resultado << ").\n";
			++fallos;
		}
		ma_decoder_uninit(&decoder);
	}

	if (archivosProbados == 0)
	{
		std::cerr << "FALLO: no se encontraron archivos OGG en " << audioDirectory << ".\n";
		return 1;
	}

	std::cout << archivosProbados << " archivos OGG probados.\n";
	return fallos == 0 ? 0 : 1;
}
