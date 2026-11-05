find_package(glm CONFIG REQUIRED)
file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" ESCAPETHEUAB_SOURCE_PATH)

add_executable(SceneLoaderTests
	SceneLoaderTests.cpp
	${PROJECT_SOURCE_DIR}/EntornVGI/SceneLoader.cpp
)
target_include_directories(SceneLoaderTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
)
target_compile_definitions(SceneLoaderTests PRIVATE
	ESCAPETHEUAB_SOURCE_DIR="${ESCAPETHEUAB_SOURCE_PATH}"
)
if(TARGET glm::glm)
	target_link_libraries(SceneLoaderTests PRIVATE glm::glm)
else()
	target_link_libraries(SceneLoaderTests PRIVATE glm)
endif()
target_compile_features(SceneLoaderTests PRIVATE cxx_std_20)

add_test(NAME SceneLoaderTests COMMAND SceneLoaderTests)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)

if(TARGET glfw)
	set(TEST_GLFW_TARGET glfw)
else()
	set(TEST_GLFW_TARGET glfw3::glfw)
endif()
if(TARGET GLEW::GLEW)
	set(TEST_GLEW_TARGET GLEW::GLEW)
else()
	set(TEST_GLEW_TARGET unofficial::glew::glew)
endif()

add_executable(ObjLoaderTests
	ObjLoaderTests.cpp
	${PROJECT_SOURCE_DIR}/EntornVGI/objLoader.cpp
	${PROJECT_SOURCE_DIR}/EntornVGI/glut_geometry.cpp
	${PROJECT_SOURCE_DIR}/EntornVGI/material.cpp
)
target_include_directories(ObjLoaderTests PRIVATE
	${PROJECT_SOURCE_DIR}/EntornVGI
	${PROJECT_SOURCE_DIR}/include/ImGui
	${PROJECT_SOURCE_DIR}/third_party/SOIL2/src
)
target_compile_definitions(ObjLoaderTests PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	ESCAPETHEUAB_SOURCE_DIR="${ESCAPETHEUAB_SOURCE_PATH}"
)
target_link_libraries(ObjLoaderTests PRIVATE
	OpenGL::GL
	${TEST_GLFW_TARGET}
	${TEST_GLEW_TARGET}
	soil2
)
target_compile_features(ObjLoaderTests PRIVATE cxx_std_20)

if(WIN32)
	add_custom_command(TARGET ObjLoaderTests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:ObjLoaderTests>
			"$<TARGET_FILE_DIR:ObjLoaderTests>"
		COMMAND_EXPAND_LISTS
	)
endif()

add_test(NAME ObjLoaderTests COMMAND ObjLoaderTests)
set_tests_properties(ObjLoaderTests PROPERTIES SKIP_RETURN_CODE 77)

if(WIN32)
	add_executable(LauncherTestStub LauncherTestStub.cpp)
	target_compile_features(LauncherTestStub PRIVATE cxx_std_20)
	add_test(NAME WindowsLauncherDoesNotRunStaleBuild
		COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
			-File "${CMAKE_CURRENT_SOURCE_DIR}/WindowsLauncherFailureTest.ps1"
			-LauncherPath "${PROJECT_SOURCE_DIR}/run-windows.ps1"
			-StubPath "$<TARGET_FILE:LauncherTestStub>"
	)
endif()
