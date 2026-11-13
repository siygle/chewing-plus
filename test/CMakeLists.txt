
configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(addon)
add_subdirectory(inputmethod)
add_executable(testchewing testchewing.cpp)
target_link_libraries(testchewing Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testchewing copy-addon copy-im)
add_test(testchewing testchewing)

add_executable(testplus testplus.cpp)
target_link_libraries(testplus Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testplus copy-addon copy-im)
# Usage: ctest -R plus  (set CHEWING_TEST_DATA / CHEWING_TEST_LIB for a
# non-system libchewing 0.14 build)
add_test(NAME plus COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-plus-test.sh
         ${PROJECT_BINARY_DIR} "$ENV{CHEWING_TEST_DATA}" "$ENV{CHEWING_TEST_LIB}")
