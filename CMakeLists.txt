cmake_minimum_required(VERSION 3.5)
# set the project name
project(dynamic_overlay)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# set(CMAKE_CXX_FLAGS " -Wall -Wextra -flto -O2")

# Set additional header files
set(SOURCE_PATH "src")
set(SOURCES
		${SOURCE_PATH}/main.cpp
		${SOURCE_PATH}/mount.h
		${SOURCE_PATH}/mount.cpp
		${SOURCE_PATH}/preinit.h
		${SOURCE_PATH}/preinit.cpp
		${SOURCE_PATH}/u-boot.h
		${SOURCE_PATH}/u-boot.cpp
		${SOURCE_PATH}/dynamic_mounting.h
		${SOURCE_PATH}/dynamic_mounting.cpp
      )
add_executable(dynamic_overlay ${SOURCES})
target_link_libraries(dynamic_overlay libinicpp.a)
target_link_libraries(dynamic_overlay libubootenv.a)
target_link_libraries(dynamic_overlay libz.a)

install(TARGETS dynamic_overlay RUNTIME DESTINATION /sbin)
