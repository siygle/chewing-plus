cmake_minimum_required(VERSION 3.6)
project(fcitx5-chewing VERSION 6.2.0)

option(CHEWING_PLUS_CORE_ONLY
       "Build libchewing-plus-core and its tests without fcitx" OFF)
option(CHEWING_PLUS_CORE_SHARED
       "Build libchewing-plus-core as a shared library (default is static)" OFF)
option(ENABLE_IBUS "Build the native IBus engine" OFF)
option(ENABLE_IBUS_SETUP
       "Build the libadwaita IBus setup program (requires ENABLE_IBUS)" OFF)
option(ENABLE_MODEL_MANAGER
       "Build the GTK4/libadwaita 語音模型 window (chewing-plus-model-manager)"
       OFF)
option(ENABLE_TEST "Build Test" On)
option(ENABLE_COVERAGE "Build the project with gcov support (Need ENABLE_TEST=On)" Off)

find_package(PkgConfig REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChewingPlusLibchewing.cmake")
include(GNUInstallDirs)

if(NOT CHEWING_PLUS_CORE_ONLY)
    set(REQUIRED_FCITX_VERSION 5.1.13)
    find_package(ECM REQUIRED 1.0.0)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

    find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
    find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend)
    # Optional push-to-talk dictation module (github.com/siygle/fcitx5-voxkit).
    find_package(Fcitx5ModuleVoxkit QUIET)
    option(ENABLE_VOXKIT "Integrate with the fcitx5-voxkit dictation module" ${Fcitx5ModuleVoxkit_FOUND})
    find_package(Gettext REQUIRED)
    include(FeatureSummary)
    include(ECMSetupVersion)
    include(ECMUninstallTarget)
    include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
endif()

add_subdirectory(src)
if(NOT CHEWING_PLUS_CORE_ONLY)
    add_subdirectory(data)
    add_subdirectory(po)
endif()

if(ENABLE_TEST)
    enable_testing()
    add_subdirectory(test/core)
    if(NOT CHEWING_PLUS_CORE_ONLY)
        add_subdirectory(test)
        if(ENABLE_COVERAGE)
            add_custom_target(coverage
                            COMMAND "${CMAKE_CTEST_COMMAND}"
                            COMMAND lcov --gcov-tool "${GCOV_TOOL}" --no-external --capture --directory ./ -b "${CMAKE_CURRENT_SOURCE_DIR}" --output-file coverage.info
                            COMMAND genhtml coverage.info --output-directory "coverage_pages"
                            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif()
    endif()
endif()

if(NOT CHEWING_PLUS_CORE_ONLY)
    fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.Chewing.metainfo.xml.in
                                  org.fcitx.Fcitx5.Addon.Chewing.metainfo.xml XML)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.Chewing.metainfo.xml" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
