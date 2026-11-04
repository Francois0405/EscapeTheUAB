# Native File Dialog vendorizado (implementación GTK3 para Linux).
cmake_minimum_required(VERSION 3.20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

add_library(nfd STATIC
	src/nfd_common.c
	src/nfd_gtk.c
)

target_include_directories(nfd
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(nfd PUBLIC PkgConfig::GTK3)

set_target_properties(nfd PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	POSITION_INDEPENDENT_CODE ON
)
