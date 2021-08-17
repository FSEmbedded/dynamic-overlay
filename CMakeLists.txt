cmake_minimum_required(VERSION 3.5)
# set the project name
project(dynamic_overlay)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Needed for SDK
#set(CMAKE_CXX_FLAGS " -Wall -Wextra -s -Os")

# Set additional header files
set(RAMDISK_STD_PATH /ramdisk)
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
		${SOURCE_PATH}/persistent_mem_detector.h
		${SOURCE_PATH}/persistent_mem_detector.cpp
		${SOURCE_PATH}/create_link.h
		${SOURCE_PATH}/create_link.cpp
      )

add_executable(dynamic_overlay ${SOURCES})

target_compile_definitions(dynamic_overlay
	PUBLIC RAMFS_MOUNTPOINT="${RAMDISK_STD_PATH}"
	RAUC_SYSTEM_CONF_PATH="${RAUC_SYSTEM_CONF_PATH}"
	NAND_RAUC_SYSTEM_CONF_PATH="${NAND_RAUC_SYSTEM_CONF_PATH}"
	EMMC_RAUC_SYSTEM_CONF_PATH="${EMMC_RAUC_SYSTEM_CONF_PATH}"
	UBOOT_ENV_PATH="${UBOOT_ENV_PATH}"
	NAND_UBOOT_ENV_PATH="${NAND_UBOOT_ENV_PATH}"
	EMMC_UBOOT_ENV_PATH="${EMMC_UBOOT_ENV_PATH}"
  )

find_library(inicpp_lib NAMES libinicpp.a libinicpp.so)
find_library(ubootenv_lib NAMES libubootenv.a libubootenv.so)
find_library(z_lib NAMES lbiz.a libz.so)

message("Path to library inicpp: ${inicpp_lib}")
message("Path to library ubootenv: ${ubootenv_lib}")
message("Path to library zlib: ${z_lib}")

target_link_libraries(dynamic_overlay
	${inicpp_lib}
	${ubootenv_lib}
	${z_lib}
  )

install(TARGETS dynamic_overlay RUNTIME DESTINATION /sbin)
install(DIRECTORY DESTINATION ${RAMDISK_STD_PATH})