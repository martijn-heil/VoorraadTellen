# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

# which compilers to use
SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH  /usr/i686-w64-mingw32/sys-root/i686-w64-mingw32/ ~/i686-w64-mingw32-install)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
