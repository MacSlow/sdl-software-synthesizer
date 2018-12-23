cmake_minimum_required (VERSION 2.8.12)
project (software-synthesizer)

set (CMAKE_CXX_COMPILER "clang++")
set (CMAKE_EXPORT_COMPILE_COMMANDS "ON")

include (FindPkgConfig)
pkg_check_modules (SDL2 sdl2 REQUIRED)
include_directories (include ${SDL2_INCLUDE_DIRS})

add_compile_options(-std=c++14 -Werror -Wall -pedantic -fno-omit-frame-pointer -Ofast -march=native)
#add_compile_options(-std=c++14 -Werror -Wall -pedantic -fno-omit-frame-pointer -fsanitize=undefined,leak,address)

add_library (ApplicationLib src/application.cpp)
add_library (GameOfLifeLib src/software-synthesizer.cpp)

add_executable (software-synthesizer src/main.cpp)
target_link_libraries (software-synthesizer
	ApplicationLib
	GameOfLifeLib
	#-fsanitize=leak,address,undefined
	${SDL2_LDFLAGS}
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
