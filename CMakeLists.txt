cmake_minimum_required(VERSION 3.5)
# set the project name
project(dynamic_overlay)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(RAUC_SYSTEM_CONF_PATH "/etc/rauc/system.conf" CACHE STRING "RAUC system.conf path")
set(NAND_RAUC_SYSTEM_CONF_PATH "/etc/rauc/system.conf.nand" CACHE STRING "NAND RAUC system.conf path")
set(EMMC_RAUC_SYSTEM_CONF_PATH "/etc/rauc/system.conf.mmc" CACHE STRING "EMMC RAUC system.conf path")
set(UBOOT_ENV_PATH "/etc/fw_env.config" CACHE STRING "U-Boot environment path")
set(EMMC_UBOOT_ENV_PATH "/etc/fw_env.config.mmc" CACHE STRING "EMMC U-Boot environment path")
set(NAND_UBOOT_ENV_PATH "/etc/fw_env.config.nand" CACHE STRING "NAND U-Boot environment path")

# Needed for SDK
#set(CMAKE_CXX_FLAGS " -Wall -Wextra -s -Os")
option(BUILD_X509_CERTIFICATE_STORE_MOUNT "Mount certificate for F&S Azure updater" OFF)

# Set additional header files
set(RAMDISK_HW_CONFIG_STD_PATH /ramdisk_hw_conf)
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
        ${SOURCE_PATH}/file_properties.h
        ${SOURCE_PATH}/file_properties.cpp
)

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    set(RAMDISK_CERT_STORE_STD_PATH /ramdisk_cert_store)
    set(TARGET_ARCHIVE_MTD_CERT_STORE ${RAMDISK_CERT_STORE_STD_PATH}/tmp.tar.bz2)

    if(NOT DEFINED TARGET_ARCHIV_DIR_PATH)
        message(FATAL_ERROR "TARGET_ARCHIV_DIR_PATH not defined")
        set(TARGET_ARCHIV_DIR_PATH ${TARGET_ARCHIV_DIR_PATH} CACHE)
    endif()

    if(NOT DEFINED TARGET_ADU_DIR_PATH)
        message(FATAL_ERROR "TARGET_ADU_DIR_PATH not defined")
        set(TARGET_ADU_DIR_PATH ${TARGET_ADU_DIR_PATH} CACHE)
    endif()

    if(NOT DEFINED SOURCE_ARCHIVE_MTD_FILE_PATH)
        message(FATAL_ERROR "SOURCE_ARCHIVE_MTD_FILE_PATH not defined")
        set(SOURCE_ARCHIVE_MTD_FILE_PATH ${SOURCE_ARCHIVE_MTD_FILE_PATH} CACHE)
    endif()

    if(NOT DEFINED SOURCE_ARCHIVE_MMC_FILE_PATH)
        message(FATAL_ERROR "SOURCE_ARCHIVE_MMC_FILE_PATH not defined")
        set(SOURCE_ARCHIVE_MMC_FILE_PATH ${SOURCE_ARCHIVE_MMC_FILE_PATH} CACHE)
    endif()

    if(NOT DEFINED FUS_AZURE_CONFIGURATION)
        message(FATAL_ERROR "FUS_AZURE_CONFIGURATION not defined")
        set(FUS_AZURE_CONFIGURATION ${FUS_AZURE_CONFIGURATION} CACHE)
    endif()

    if(NOT DEFINED FUS_AZURE_CERT_CERTIFICATE_NAME)
        message(FATAL_ERROR "FUS_AZURE_CERT_CERTIFICATE_NAME not defined")
        set(FUS_AZURE_CERT_CERTIFICATE_NAME ${FUS_AZURE_CERT_CERTIFICATE_NAME} CACHE)
    endif()

    if(NOT DEFINED FUS_AZURE_CERT_KEY_NAME)
        message(FATAL_ERROR "FUS_AZURE_CERT_KEY_NAME not defined")
        set(FUS_AZURE_CERT_KEY_NAME ${FUS_AZURE_CERT_KEY_NAME} CACHE)
    endif()

    if(NOT DEFINED PART_NAME_MTD_CERT)
        message(FATAL_ERROR "PART_NAME_MTD_CERT not defined")
        set(PART_NAME_MTD_CERT ${PART_NAME_MTD_CERT} CACHE)
    endif()

    set(SOURCES
            ${SOURCES}
            ${SOURCE_PATH}/x509_cert_store.h
            ${SOURCE_PATH}/x509_cert_store.cpp
        )
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

if(NOT DEFINED RAUC_SYSTEM_CONF_PATH)
    if(DEFINED ENV{RAUC_SYSTEM_CONF_PATH})
        set(RAUC_SYSTEM_CONF_PATH $ENV{RAUC_SYSTEM_CONF_PATH} CACHE STRING "RAUC system.conf path")
        message(STATUS "Using RAUC_SYSTEM_CONF_PATH from environment: ${RAUC_SYSTEM_CONF_PATH}")
    else()
        message(FATAL_ERROR "RAUC_SYSTEM_CONF_PATH not defined in CMake or environment!")
    endif()
endif()

if(NOT DEFINED NAND_RAUC_SYSTEM_CONF_PATH)
    if(DEFINED ENV{NAND_RAUC_SYSTEM_CONF_PATH})
        set(NAND_RAUC_SYSTEM_CONF_PATH $ENV{NAND_RAUC_SYSTEM_CONF_PATH} CACHE STRING "NAND RAUC system.conf path")
        message(STATUS "Using NAND_RAUC_SYSTEM_CONF_PATH from environment: ${NAND_RAUC_SYSTEM_CONF_PATH}")
    else()
        message(FATAL_ERROR "NAND_RAUC_SYSTEM_CONF_PATH not defined in CMake or environment!")
    endif()
endif()

if(NOT DEFINED EMMC_RAUC_SYSTEM_CONF_PATH)
    if(DEFINED ENV{EMMC_RAUC_SYSTEM_CONF_PATH})
        set(EMMC_RAUC_SYSTEM_CONF_PATH $ENV{EMMC_RAUC_SYSTEM_CONF_PATH} CACHE STRING "EMMC RAUC system.conf path")
        message(STATUS "Using EMMC_RAUC_SYSTEM_CONF_PATH from environment: ${EMMC_RAUC_SYSTEM_CONF_PATH}")
    else()
        message(FATAL_ERROR "EMMC_RAUC_SYSTEM_CONF_PATH not defined")
    endif()
endif()

if(NOT DEFINED UBOOT_ENV_PATH)
    if(DEFINED ENV{UBOOT_ENV_PATH})
        set(UBOOT_ENV_PATH $ENV{UBOOT_ENV_PATH} CACHE STRING "U-Boot environment path")
        message(STATUS "Using UBOOT_ENV_PATH from environment: ${UBOOT_ENV_PATH}")
    else()
        message(FATAL_ERROR "UBOOT_ENV_PATH not defined in CMake or environment!")
    endif()
endif()

if(NOT DEFINED EMMC_UBOOT_ENV_PATH)
    if(DEFINED ENV{EMMC_UBOOT_ENV_PATH})
        set(EMMC_UBOOT_ENV_PATH $ENV{EMMC_UBOOT_ENV_PATH} CACHE STRING "EMMC U-Boot environment path")
        message(STATUS "Using EMMC_UBOOT_ENV_PATH from environment: ${EMMC_UBOOT_ENV_PATH}")
    else()
        message(FATAL_ERROR "EMMC_UBOOT_ENV_PATH not defined in CMake or environment!")
    endif()
endif()

if(NOT DEFINED NAND_UBOOT_ENV_PATH)
    if(DEFINED ENV{NAND_UBOOT_ENV_PATH})
        set(NAND_UBOOT_ENV_PATH $ENV{NAND_UBOOT_ENV_PATH} CACHE STRING "NAND U-Boot environment path")
        message(STATUS "Using NAND_UBOOT_ENV_PATH from environment: ${NAND_UBOOT_ENV_PATH}")
    else()
        message(FATAL_ERROR "NAND_UBOOT_ENV_PATH not defined")
    endif()
endif()

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        BUILD_X509_CERTIFICATE_STORE_MOUNT
        TARGET_ARCHIV_DIR_PATH="${TARGET_ARCHIV_DIR_PATH}"
        TARGET_ADU_DIR_PATH="${TARGET_ADU_DIR_PATH}"
        RAMFS_CERT_STORE_MOUNTPOINT="${RAMDISK_CERT_STORE_STD_PATH}"
        SOURCE_ARCHIVE_MTD_FILE_PATH="${SOURCE_ARCHIVE_MTD_FILE_PATH}"
        SOURCE_ARCHIVE_MMC_FILE_PATH="${SOURCE_ARCHIVE_MMC_FILE_PATH}"
        FUS_AZURE_CONFIGURATION="${FUS_AZURE_CONFIGURATION}"
        FUS_AZURE_CERT_CERTIFICATE_NAME="${FUS_AZURE_CERT_CERTIFICATE_NAME}"
        FUS_AZURE_CERT_KEY_NAME="${FUS_AZURE_CERT_KEY_NAME}"
        TARGET_ARCHIVE_MTD_CERT_STORE="${TARGET_ARCHIVE_MTD_CERT_STORE}"
        PART_NAME_MTD_CERT="${PART_NAME_MTD_CERT}"
    )
endif()

if(DEFINED PERSISTMEMORY_REGEX_EMMC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        PERSISTMEMORY_REGEX_EMMC="${PERSISTMEMORY_REGEX_EMMC}"
    )
endif()

if(DEFINED PERSISTMEMORY_REGEX_NAND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        PERSISTMEMORY_REGEX_NAND="${PERSISTMEMORY_REGEX_NAND}"
    )
endif()

if(DEFINED PERSISTMEMORY_DEVICE_NAME)
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        PERSISTMEMORY_DEVICE_NAME="${PERSISTMEMORY_DEVICE_NAME}"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
    RAMFS_HW_CONFIG_MOUNTPOINT="${RAMDISK_HW_CONFIG_STD_PATH}"
    RAUC_SYSTEM_CONF_PATH="${RAUC_SYSTEM_CONF_PATH}"
    NAND_RAUC_SYSTEM_CONF_PATH="${NAND_RAUC_SYSTEM_CONF_PATH}"
    EMMC_RAUC_SYSTEM_CONF_PATH="${EMMC_RAUC_SYSTEM_CONF_PATH}"
    UBOOT_ENV_PATH="${UBOOT_ENV_PATH}"
    NAND_UBOOT_ENV_PATH="${NAND_UBOOT_ENV_PATH}"
    EMMC_UBOOT_ENV_PATH="${EMMC_UBOOT_ENV_PATH}"
    EMMC_SECURE_PART_BLK_NR=${EMMC_SECURE_PART_BLK_NR}
)

find_library(inicpp_lib NAMES libinicpp.a libinicpp.so)
find_library(ubootenv_lib NAMES libubootenv.so)
find_library(z_lib NAMES lbiz.a libz.so)
find_library(jsoncpp_lib NAMES libjsoncpp_static.a libjsoncpp.so)
find_library(blkid_lib NAMES libblkid.a libblkid.so)

if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    find_library(libjsoncpp NAMES libjsoncpp_static.a)
endif(BUILD_X509_CERTIFICATE_STORE_MOUNT)

message("Path to library inicpp: ${inicpp_lib}")
message("Path to library ubootenv: ${ubootenv_lib}")
message("Path to library zlib: ${z_lib}")
message("Path to library jsoncpp: ${jsoncpp_lib}")
message("Path to library blkid: ${blkid_lib}")

target_link_libraries(${PROJECT_NAME}
    ${inicpp_lib}
    ${ubootenv_lib}
    ${z_lib}
    ${jsoncpp_lib}
    ${blkid_lib}
)

install(TARGETS dynamic_overlay RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(DIRECTORY DESTINATION ${RAMDISK_HW_CONFIG_STD_PATH})
if(BUILD_X509_CERTIFICATE_STORE_MOUNT)
    install(DIRECTORY DESTINATION ${RAMDISK_CERT_STORE_STD_PATH})
endif()
