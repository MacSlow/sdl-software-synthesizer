cmake_minimum_required (VERSION 2.8.12)
project (software-synthesizer)

set (CMAKE_CXX_COMPILER "clang++")
set (CMAKE_EXPORT_COMPILE_COMMANDS "ON")

include (FindPkgConfig)
pkg_check_modules (SDL2 sdl2 REQUIRED)
pkg_check_modules (GLEW glew REQUIRED)
pkg_check_modules (ALSA alsa REQUIRED)
include_directories (include
					 ${SDL2_INCLUDE_DIRS}
					 ${GLEW_INCLUDE_DIRS}
					 ${ALSA_INCLUDE_DIRS})

add_compile_options(-std=c++17 -Werror -Wall -pedantic -Os)
#add_compile_options(-std=c++17 -Werror -Wall -pedantic -O0 -ggdb -fsanitize=undefined,leak,address)

add_library (ApplicationLib src/application.cpp)
add_library (OpenGLLib src/opengl.cpp)
add_library (MidiLib src/midi.cpp)
add_library (FiltersLib src/filters.cpp)

add_executable (software-synthesizer src/main.cpp)
target_link_libraries (software-synthesizer
	ApplicationLib
	OpenGLLib
	MidiLib
	FiltersLib
	GL
	#-fsanitize=leak,address,undefined
	${SDL2_LDFLAGS}
	${GLEW_LIBRARIES}
	${ALSA_LIBRARIES}
	-pthread
	-s
)

add_custom_target (valgrind
	DEPENDS software-synthesizer
	COMMAND valgrind --track-origins=yes --show-leak-kinds=all  --leak-check=full -v ./software-synthesizer
)

add_custom_target (perf
	DEPENDS software-synthesizer
	COMMAND perf stat ./software-synthesizer
)

add_custom_target (report
	DEPENDS software-synthesizer
	COMMAND perf record -g ./software-synthesizer
	COMMAND perf report -g 'graph,0.5,caller' --sort comm,dso,sym
)

add_custom_target (tiny
	DEPENDS software-synthesizer
	COMMAND sstrip ./software-synthesizer
	COMMAND gzip -cn9 ./software-synthesizer >small
	COMMAND cat ../data/tiny ./small >compact
	COMMAND chmod +x ./compact
	COMMAND rm ./small ./software-synthesizer
	COMMAND mv ./compact ./software-synthesizer
)

add_custom_target (tidy
	DEPENDS software-synthesizer
	COMMAND clang-tidy -p=. ../src/*.cpp
)
