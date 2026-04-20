cmake_minimum_required(VERSION 3.18)
project(dynamic_overlay VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

# ==============================================================================
# Options
# ==============================================================================

option(ENABLE_LOGGING       "Enable logging output"                        ON)
option(ENABLE_DEBUG_LOGGING "Enable debug-level logging"                   OFF)
option(ENABLE_SANITIZERS    "Enable ASan and UBSan (Debug builds)"         OFF)
option(BUILD_TESTING        "Build unit tests"                             OFF)
option(BUILD_MAIN_TARGET    "Build main executable (OFF = tests only)"     ON)
option(BUILD_X509_CERTIFICATE_STORE_MOUNT "Build X.509 certificate store"  OFF)

set(LOG_BACKEND  "STDERR" CACHE STRING "Log backend: KMSG or STDERR")
set(OPTIMIZE_FOR "SIZE"   CACHE STRING "Release optimization: SIZE or SPEED")
set_property(CACHE LOG_BACKEND  PROPERTY STRINGS KMSG STDERR)
set_property(CACHE OPTIMIZE_FOR PROPERTY STRINGS SIZE SPEED)

# Validate enum options
if(NOT OPTIMIZE_FOR MATCHES "^(SIZE|SPEED)$")
    message(FATAL_ERROR "OPTIMIZE_FOR must be SIZE or SPEED, got: ${OPTIMIZE_FOR}")
endif()
if(NOT LOG_BACKEND MATCHES "^(KMSG|STDERR)$")
    message(FATAL_ERROR "LOG_BACKEND must be KMSG or STDERR, got: ${LOG_BACKEND}")
endif()

# Override CMake's default Release flags (-O3 -DNDEBUG) to avoid conflicting -O levels.
# Our OPTIMIZE_FOR option controls the optimization flag explicitly.
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "" FORCE)

# ==============================================================================
# Path configuration (override via -D or Yocto recipe)
# ==============================================================================

# Overlay mounting
set(PATH_TO_MOUNT_APPIMAGE     "/rw_fs/root/application/current"             CACHE STRING "Application image mount point")
set(DEFAULT_OVERLAY_PATH        "/rw_fs/root/application/current/overlay.ini" CACHE STRING "Overlay INI config path")
set(DEFAULT_APPLICATION_PATH    "/rw_fs/root/application/current"             CACHE STRING "Application directory path")
set(DEFAULT_UPPERDIR_PATH       "/rw_fs/root/upperdir"                        CACHE STRING "Overlay upper directory path")
set(DEFAULT_WORKDIR_PATH        "/rw_fs/root/workdir"                         CACHE STRING "Overlay work directory path")
set(APP_IMAGE_DIR               "/rw_fs/root/application/"                    CACHE STRING "Application image directory")
set(MAX_OVERLAY_COUNT           "8"                                           CACHE STRING "Maximum number of overlay mounts")

# Persistent memory
set(PERSISTENT_MEMORY_MOUNTPOINT "/rw_fs/root" CACHE STRING "Persistent memory mount point")
set(PERSISTMEMORY_DEVICE_NAME   "data"         CACHE STRING "Persistent memory device label")

# RAUC
set(RAUC_SYSTEM_CONF_PATH      "/etc/rauc/system.conf"      CACHE STRING "RAUC system.conf path")
set(NAND_RAUC_SYSTEM_CONF_PATH "/etc/rauc/system.conf.nand" CACHE STRING "NAND RAUC system.conf path")
set(EMMC_RAUC_SYSTEM_CONF_PATH "/etc/rauc/system.conf.mmc"  CACHE STRING "eMMC RAUC system.conf path")

# U-Boot environment
set(UBOOT_ENV_PATH      "/etc/fw_env.config"      CACHE STRING "U-Boot environment config path")
set(EMMC_UBOOT_ENV_PATH "/etc/fw_env.config.mmc"  CACHE STRING "eMMC U-Boot environment path")
set(NAND_UBOOT_ENV_PATH "/etc/fw_env.config.nand" CACHE STRING "NAND U-Boot environment path")

# ==============================================================================
# X.509 certificate store configuration
# ==============================================================================

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    set(TARGET_ARCHIV_DIR_PATH           "" CACHE STRING "X.509 certificate target directory (tmpfs mount point)")
    set(FUS_AZURE_CONFIGURATION          "" CACHE STRING "Azure DU config path")
    set(PART_NAME_MTD_CERT               "" CACHE STRING "MTD certificate partition name")
    set(EMMC_SECURE_PART_BLK_NR          "" CACHE STRING "eMMC secure partition block number")

    # Validate required variables
    foreach(_var
        TARGET_ARCHIV_DIR_PATH
        FUS_AZURE_CONFIGURATION
        PART_NAME_MTD_CERT
        EMMC_SECURE_PART_BLK_NR
    )
        if("${${_var}}" STREQUAL "")
            message(FATAL_ERROR "${_var} must be set when BUILD_X509_CERTIFICATE_STORE_MOUNT is ON")
        endif()
    endforeach()
endif()

# ==============================================================================
# Generated config header
# ==============================================================================

configure_file(src/config.h.in ${CMAKE_BINARY_DIR}/generated/config.h @ONLY)

# ==============================================================================
# Dependencies (find before target — fail early on missing libraries)
# ==============================================================================

if(BUILD_MAIN_TARGET)
    find_library(ubootenv_lib NAMES ubootenv REQUIRED)
    find_library(blkid_lib    NAMES blkid    REQUIRED)

    if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
        find_library(jsoncpp_lib  NAMES jsoncpp  REQUIRED)
        find_library(archive_lib  NAMES archive  REQUIRED)
    endif()
endif()

# ==============================================================================
# Tests-only mode (skip main executable when cross-compile libs unavailable)
# ==============================================================================

if(BUILD_TESTING AND NOT BUILD_MAIN_TARGET)
    enable_testing()
    add_subdirectory(tests)
    return()
endif()

# ==============================================================================
# Sources
# ==============================================================================

set(SOURCES
    src/main.cpp
    src/posix_utils.cpp
    src/ini_parser.cpp
    src/mount.cpp
    src/preinit.cpp
    src/u-boot.cpp
    src/dynamic_mounting.cpp
    src/overlay_config.cpp
    src/persistent_mem_detector.cpp
    src/device_parser.cpp
    src/create_link.cpp
    src/file_properties.cpp
)

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    list(APPEND SOURCES src/x509_cert_store.cpp)
endif()

# ==============================================================================
# Target
# ==============================================================================

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# ==============================================================================
# Compiler and linker flags
# ==============================================================================

target_compile_options(${PROJECT_NAME} PRIVATE
    # Base flags
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Wpedantic
    # Release optimizations
    $<$<CONFIG:Release>:$<IF:$<STREQUAL:${OPTIMIZE_FOR},SPEED>,-O2,-Os>>
    $<$<CONFIG:Release>:-flto>
    $<$<CONFIG:Release>:-ffunction-sections>
    $<$<CONFIG:Release>:-fdata-sections>
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:-Wl,--gc-sections>
    $<$<CONFIG:Release>:-flto>
    $<$<CONFIG:Release>:-s>
)

if(ENABLE_SANITIZERS)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
    )
endif()

# ==============================================================================
# Compile definitions
# ==============================================================================

if(ENABLE_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_LOGGING)
    if(LOG_BACKEND STREQUAL "KMSG")
        target_compile_definitions(${PROJECT_NAME} PRIVATE LOG_BACKEND_KMSG)
    endif()
endif()

if(ENABLE_DEBUG_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DEBUG_LOGGING)
endif()

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_X509_CERTIFICATE_STORE_MOUNT)
endif()

# ==============================================================================
# Link dependencies
# ==============================================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${ubootenv_lib}
    ${blkid_lib}
)

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${jsoncpp_lib} ${archive_lib})
endif()

# ==============================================================================
# Install
# ==============================================================================

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    install(DIRECTORY DESTINATION ${TARGET_ARCHIV_DIR_PATH})
endif()

# ==============================================================================
# Tests
# ==============================================================================

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
