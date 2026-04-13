/**
 * @file logging.h
 * @brief Conditional logging macros for dynamic_overlay
 *
 * Provides compile-time configurable logging macros that can be disabled
 * to reduce binary size and improve startup time in production builds.
 *
 * @section Usage
 *
 * Enable logging by defining ENABLE_LOGGING before including this header
 * or via compiler flag: -DENABLE_LOGGING
 *
 * @code
 * #define ENABLE_LOGGING
 * #include "logging.h"
 *
 * LOG_INFO("Application started");
 * LOG_DEBUG("Processing entry: " << entry);
 * LOG_WARNING("File not found: " << path);
 * LOG_ERROR("Mount failed: " << strerror(errno));
 * @endcode
 *
 * @section Levels
 *
 * - LOG_DEBUG   - Detailed debugging information (disabled in release)
 * - LOG_INFO    - General informational messages
 * - LOG_WARNING - Warning conditions
 * - LOG_ERROR   - Error conditions
 *
 * @note All logging outputs to std::cerr with "dynamicoverlay: " prefix
 */

#ifndef DYNAMIC_OVERLAY_LOGGING_H
#define DYNAMIC_OVERLAY_LOGGING_H

#include <iostream>

/**
 * @def ENABLE_LOGGING
 * @brief Master switch to enable/disable all logging
 *
 * Define this macro before including logging.h or pass -DENABLE_LOGGING
 * to the compiler to enable logging output.
 */

/**
 * @def ENABLE_DEBUG_LOGGING
 * @brief Enable verbose debug logging
 *
 * Only effective when ENABLE_LOGGING is also defined.
 * Use for development and debugging only.
 */

#ifdef ENABLE_LOGGING

    /**
     * @def LOG_PREFIX
     * @brief Prefix for all log messages
     */
    #define LOG_PREFIX "dynamicoverlay: "

    /**
     * @def LOG_ERROR(msg)
     * @brief Log an error message
     * @param msg Message to log (can use stream operators)
     */
    #define LOG_ERROR(msg) \
        std::cerr << LOG_PREFIX << "ERROR: " << msg << std::endl

    /**
     * @def LOG_WARNING(msg)
     * @brief Log a warning message
     * @param msg Message to log (can use stream operators)
     */
    #define LOG_WARNING(msg) \
        std::cerr << LOG_PREFIX << "WARNING: " << msg << std::endl

    /**
     * @def LOG_INFO(msg)
     * @brief Log an informational message
     * @param msg Message to log (can use stream operators)
     */
    #define LOG_INFO(msg) \
        std::cerr << LOG_PREFIX << "INFO: " << msg << std::endl

    #ifdef ENABLE_DEBUG_LOGGING
        /**
         * @def LOG_DEBUG(msg)
         * @brief Log a debug message (only when ENABLE_DEBUG_LOGGING is defined)
         * @param msg Message to log (can use stream operators)
         */
        #define LOG_DEBUG(msg) \
            std::cerr << LOG_PREFIX << "DEBUG: " << msg << std::endl
    #else
        #define LOG_DEBUG(msg) ((void)0)
    #endif

#else
    /* Logging disabled - all macros expand to nothing */
    #define LOG_ERROR(msg)   ((void)0)
    #define LOG_WARNING(msg) ((void)0)
    #define LOG_INFO(msg)    ((void)0)
    #define LOG_DEBUG(msg)   ((void)0)
#endif

/**
 * @def LOG_ERRNO(msg)
 * @brief Log an error message with errno description
 * @param msg Message prefix (can use stream operators)
 *
 * Automatically appends ": <errno description>" to the message.
 * Always enabled regardless of ENABLE_LOGGING.
 */
#define LOG_ERRNO(msg) \
    std::cerr << "dynamicoverlay: " << msg << ": " << strerror(errno) << std::endl

#endif /* DYNAMIC_OVERLAY_LOGGING_H */
