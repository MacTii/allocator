#
# DANTE :: System Automatycznych Testów Jednostkowych oraz Akceptacyjnych
# Tomasz Jaworski, 2018-2020
#
# Plik wygenerowany automatycznie
# Znacznik czasowy: 2022-12-10 20:09:21.559476
#

cmake_minimum_required(VERSION 3.16.3)
project(Allocator C)

# Przyjmij standard C11
set(CMAKE_C_STANDARD 11)

# Ustaw opcje kompilatora (z raportu Dante)
add_compile_options(
      "-Wno-error=parentheses"
      "-D_ANSI_OUTPUT"
      "-Wextra"
      "-pedantic"
      "-Wno-parentheses"
      "-D_GNU_SOURCE"
      "-Wno-error=unused-parameter"
      "-std=c11"
      "-Wall"
      "-ggdb3"
      "-DINSIDE_DANTE"
      "-Werror=vla"
      "-Wno-error=implicit-fallthrough"
      "-fdiagnostics-color"
      "-xc"
      "-Werror"
      "-fmax-errors=5"

)

# Ustaw opcje konsolidatora (z raportu Dante)
add_link_options(
        "-Wl,-cref"
        "-Wl,-Map=main.map"
        "-Wl,-wrap,main"
        "-ggdb3"
)

# Utwórz target 'project1' i dodaj do niego pliki źródlowe:
add_executable(Allocator
        "heap.c"
        "main.c"
        "unit_helper_v2.c"
        "unit_test_v2.c"
        "rdebug.c"
        "memmanager.c"
        )

# Dołącz niezbędne biblioteki
target_link_libraries(Allocator
        "m"
        "pthread"
)