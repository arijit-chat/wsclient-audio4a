message ("*** Compiling in debug mode")

# Compilation Mode (DEBUG, RELEASE)
# ----------------------------------
set(CMAKE_BUILD_TYPE "DEBUG")
set(USE_EFENCE 1)

add_compile_options(-DDEBUGMODE)
