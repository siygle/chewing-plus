# Pinned libchewing for chewing-plus.
# Commit 1bfcde9078f13b4d97aaa4b6a01cbd67687522e3 (v0.14.0-rc.2).
# Source: https://codeberg.org/chewing/libchewing
set(CHEWING_PLUS_LIBCHEWING_VERSION "0.14.0-rc.2")
set(CHEWING_PLUS_LIBCHEWING_TAG "v0.14.0-rc.2")
set(CHEWING_PLUS_LIBCHEWING_URL "https://codeberg.org/chewing/libchewing")
set(CHEWING_PLUS_LIBCHEWING_COMMIT "1bfcde9078f13b4d97aaa4b6a01cbd67687522e3")

option(CHEWING_PLUS_BUNDLE_LIBCHEWING
       "Use libchewing from CHEWING_PLUS_LIBCHEWING_PREFIX (see scripts/bundle-libchewing.sh)"
       OFF)
set(CHEWING_PLUS_LIBCHEWING_PREFIX "${CMAKE_SOURCE_DIR}/third_party/dist"
    CACHE PATH "Prefix produced by scripts/bundle-libchewing.sh")
option(CHEWING_PLUS_LIBCHEWING_STATIC
       "Statically link the bundled libchewing (prefix must be a --static build)"
       OFF)

if(NOT DEFINED CHEWING_TARGET)
    if(CHEWING_PLUS_BUNDLE_LIBCHEWING)
        set(_chewing_pc "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib/pkgconfig")
        if(EXISTS "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib64/pkgconfig/chewing.pc")
            set(_chewing_pc "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib64/pkgconfig")
        endif()
        set(ENV{PKG_CONFIG_PATH} "${_chewing_pc}:$ENV{PKG_CONFIG_PATH}")
    endif()

    if(CHEWING_PLUS_BUNDLE_LIBCHEWING AND CHEWING_PLUS_LIBCHEWING_STATIC)
        find_library(CHEWING_PLUS_STATIC_LIB NAMES libchewing.a chewing
                     PATHS "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib"
                           "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib64"
                     NO_DEFAULT_PATH REQUIRED)
        find_library(CHEWING_PLUS_CAPI_LIB NAMES libchewing_capi.a chewing_capi
                     PATHS "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib"
                           "${CHEWING_PLUS_LIBCHEWING_PREFIX}/lib64"
                     NO_DEFAULT_PATH)
        find_path(CHEWING_PLUS_INCLUDE_DIR chewing.h
                  PATHS "${CHEWING_PLUS_LIBCHEWING_PREFIX}/include"
                  PATH_SUFFIXES chewing
                  NO_DEFAULT_PATH REQUIRED)
        add_library(chewing-plus-bundled STATIC IMPORTED)
        set_target_properties(chewing-plus-bundled PROPERTIES
                              IMPORTED_LOCATION "${CHEWING_PLUS_STATIC_LIB}"
                              INTERFACE_INCLUDE_DIRECTORIES "${CHEWING_PLUS_INCLUDE_DIR}")
        set(_chewing_extra "${CMAKE_DL_LIBS}" m pthread)
        if(CHEWING_PLUS_CAPI_LIB)
            list(APPEND _chewing_extra "${CHEWING_PLUS_CAPI_LIB}")
        endif()
        set_property(TARGET chewing-plus-bundled APPEND PROPERTY
                     INTERFACE_LINK_LIBRARIES "${_chewing_extra}")
        set(CHEWING_TARGET chewing-plus-bundled)
        message(STATUS "chewing-plus: static libchewing ${CHEWING_PLUS_STATIC_LIB}")
    else()
        pkg_check_modules(Chewing "chewing>=0.5.0" IMPORTED_TARGET REQUIRED)
        set(CHEWING_TARGET PkgConfig::Chewing)
        message(STATUS "chewing-plus: libchewing ${Chewing_VERSION} (pin ${CHEWING_PLUS_LIBCHEWING_VERSION})")
    endif()
endif()
