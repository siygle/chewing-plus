add_library(voxkit-protocol STATIC voxkit/protocol.c voxkit/client.c)
target_include_directories(voxkit-protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(voxkit-protocol PROPERTIES
    OUTPUT_NAME voxkit-protocol
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
install(TARGETS voxkit-protocol ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../include/voxkit_protocol.h"
              "${CMAKE_CURRENT_SOURCE_DIR}/../include/voxkit_client.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

add_executable(chewing-plus-models models-cli.c)
target_link_libraries(chewing-plus-models PRIVATE voxkit-protocol)
set_target_properties(chewing-plus-models PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_NAME chewing-plus-models)
install(TARGETS chewing-plus-models RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

set(_core_type STATIC)
if(CHEWING_PLUS_CORE_SHARED)
    set(_core_type SHARED)
endif()

add_library(chewing-plus-core ${_core_type}
    plus/ChewingPlus.cpp
    plus/PlusEmoji.cpp
    plus/PlusEnglish.cpp
    plus/PlusFeatures.cpp
    plus/PlusPhrases.cpp
    plus/PlusData.cpp
    abi/chewing_plus.cpp
)
target_link_libraries(chewing-plus-core PUBLIC ${CHEWING_TARGET})
target_include_directories(chewing-plus-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plus
)
set_target_properties(chewing-plus-core PROPERTIES
    OUTPUT_NAME chewing-plus-core
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
if(CHEWING_PLUS_CORE_SHARED)
    set_target_properties(chewing-plus-core PROPERTIES
        VERSION 1.0.0
        SOVERSION 1
    )
endif()
target_compile_definitions(chewing-plus-core PRIVATE CHEWING_PLUS_BUILDING=1)

# data/plus-phrases.txt -> plus-phrases.bin, memory-mapped at run time.
add_executable(chewing-plus-compile-phrases plus/compile-phrases.cpp)
target_link_libraries(chewing-plus-compile-phrases PRIVATE chewing-plus-core)
target_include_directories(chewing-plus-compile-phrases PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/plus")
set_target_properties(chewing-plus-compile-phrases PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
set(CHEWING_PLUS_PHRASES_BIN "${CMAKE_BINARY_DIR}/plus-phrases.bin"
    CACHE INTERNAL "Compiled phrase index")
add_custom_command(
    OUTPUT "${CHEWING_PLUS_PHRASES_BIN}"
    COMMAND chewing-plus-compile-phrases
            "${CMAKE_SOURCE_DIR}/data/plus-phrases.txt"
            "${CHEWING_PLUS_PHRASES_BIN}"
    DEPENDS chewing-plus-compile-phrases "${CMAKE_SOURCE_DIR}/data/plus-phrases.txt"
    COMMENT "Compiling plus-phrases.bin")
add_custom_target(chewing-plus-phrases ALL DEPENDS "${CHEWING_PLUS_PHRASES_BIN}")

install(TARGETS chewing-plus-core
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../include/chewing_plus.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(NOT CHEWING_PLUS_CORE_ONLY)
    set(CHEWING_SOURCES eim.cpp)
    add_fcitx5_addon(chewing ${CHEWING_SOURCES})
    target_link_libraries(chewing Fcitx5::Core Fcitx5::Config chewing-plus-core
                          voxkit-protocol ${CHEWING_TARGET})
    target_include_directories(chewing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plus)
    if(ENABLE_VOXKIT)
        target_link_libraries(chewing Fcitx5::Module::Voxkit)
        target_compile_definitions(chewing PRIVATE CHEWING_WITH_VOXKIT=1)
        message(STATUS "fcitx5-chewing: voxkit dictation integration enabled")
    endif()
#Plus sources use C++ 20(std::string::starts_with).The addon includes
#ChewingPlus.h, so it needs the same standard.
    set_target_properties(chewing PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_compile_definitions(chewing PRIVATE FCITX_GETTEXT_DOMAIN=\"fcitx5-chewing\")
    fcitx5_add_i18n_definition(TARGETS chewing)
    install(TARGETS chewing DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
    fcitx5_translate_desktop_file(chewing.conf.in chewing.conf)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chewing.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod" COMPONENT config)
    configure_file(chewing-addon.conf.in.in chewing-addon.conf.in)
    fcitx5_translate_desktop_file("${CMAKE_CURRENT_BINARY_DIR}/chewing-addon.conf.in" chewing-addon.conf)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chewing-addon.conf" RENAME chewing.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" COMPONENT config)
endif()

add_library(chewing-plus-ibus-prefs STATIC ibus/prefs.c)
target_include_directories(chewing-plus-ibus-prefs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ibus")
set_target_properties(chewing-plus-ibus-prefs PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)

add_library(chewing-plus-ibus-keys STATIC ibus/keys.c)
target_include_directories(chewing-plus-ibus-keys PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ibus"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(chewing-plus-ibus-keys PUBLIC chewing-plus-core)
set_target_properties(chewing-plus-ibus-keys PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)

if(ENABLE_IBUS_SETUP AND NOT ENABLE_IBUS)
    message(FATAL_ERROR "ENABLE_IBUS_SETUP requires ENABLE_IBUS")
endif()

if(ENABLE_IBUS)
    pkg_check_modules(IBus REQUIRED ibus-1.0)
    set(CHEWING_PLUS_IBUS_EXEC "${CMAKE_INSTALL_FULL_LIBEXECDIR}/ibus-chewing-plus")
    if(ENABLE_IBUS_SETUP)
        set(CHEWING_PLUS_IBUS_SETUP_LINE
            "<setup>${CMAKE_INSTALL_FULL_LIBEXECDIR}/ibus-setup-chewing-plus</setup>")
    else()
        set(CHEWING_PLUS_IBUS_SETUP_LINE "")
    endif()
    configure_file(ibus/chewing-plus.xml.in chewing-plus.xml @ONLY)
    add_executable(ibus-chewing-plus ibus/main.c ibus/engine.c)
    target_include_directories(ibus-chewing-plus PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/ibus" ${IBus_INCLUDE_DIRS})
    target_compile_options(ibus-chewing-plus PRIVATE ${IBus_CFLAGS_OTHER})
    target_link_libraries(ibus-chewing-plus PRIVATE
        chewing-plus-ibus-keys chewing-plus-ibus-prefs voxkit-protocol
        ${IBus_LIBRARIES})
    target_compile_definitions(ibus-chewing-plus PRIVATE
        CHEWING_PLUS_IBUS_EMOJI="${CMAKE_INSTALL_FULL_DATADIR}/chewing-plus/plus-emoji-shortcodes.txt"
        CHEWING_PLUS_IBUS_ENGLISH="${CMAKE_INSTALL_FULL_DATADIR}/chewing-plus/plus-english.txt"
        CHEWING_PLUS_IBUS_PHRASES="${CMAKE_INSTALL_FULL_DATADIR}/chewing-plus/plus-phrases.bin")
    set_target_properties(ibus-chewing-plus PROPERTIES
        C_STANDARD 99 C_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_NAME ibus-chewing-plus)
    install(TARGETS ibus-chewing-plus RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/chewing-plus.xml"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/ibus/component")
    install(FILES "${CMAKE_SOURCE_DIR}/data/plus-emoji-shortcodes.txt"
                  "${CMAKE_SOURCE_DIR}/data/plus-english.txt"
                  "${CHEWING_PLUS_PHRASES_BIN}"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/chewing-plus")
    if(ENABLE_IBUS_SETUP)
        pkg_check_modules(Adw REQUIRED libadwaita-1)
        add_executable(ibus-setup-chewing-plus ibus/setup.c)
        target_include_directories(ibus-setup-chewing-plus PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/ibus" ${Adw_INCLUDE_DIRS})
        target_compile_options(ibus-setup-chewing-plus PRIVATE
            ${Adw_CFLAGS_OTHER})
        target_link_libraries(ibus-setup-chewing-plus PRIVATE
            chewing-plus-ibus-prefs chewing-plus-core voxkit-protocol
            ${Adw_LIBRARIES})
        set_target_properties(ibus-setup-chewing-plus PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON
            LINKER_LANGUAGE CXX
            RUNTIME_OUTPUT_NAME ibus-setup-chewing-plus)
        install(TARGETS ibus-setup-chewing-plus
                RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
    endif()
endif()
