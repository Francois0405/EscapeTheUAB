# SOIL2 vendorizado para Linux (la versión de Windows se sigue tomando de
# include/gl y lib/x64 en el proyecto de Visual Studio).
cmake_minimum_required(VERSION 3.20)

add_library(soil2 STATIC
	src/SOIL2.c
	src/image_DXT.c
	src/image_helper.c
	src/image_array.c
	src/wfETC.c
)

target_include_directories(soil2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(soil2 PUBLIC OpenGL::GL ${CMAKE_DL_LIBS})

set_target_properties(soil2 PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON
)
