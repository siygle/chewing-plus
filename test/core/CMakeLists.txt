add_executable(test_ibus_prefs test_ibus_prefs.c)
target_link_libraries(test_ibus_prefs PRIVATE chewing-plus-ibus-prefs)
add_test(NAME ibus-prefs COMMAND test_ibus_prefs)

add_executable(test_ibus_keys test_ibus_keys.c)
target_link_libraries(test_ibus_keys PRIVATE chewing-plus-ibus-keys)
set_target_properties(test_ibus_keys PROPERTIES LINKER_LANGUAGE CXX)
add_test(NAME ibus-keys COMMAND test_ibus_keys)

find_package(Threads REQUIRED)
add_executable(test_voxkit_client test_voxkit_client.c)
target_link_libraries(test_voxkit_client PRIVATE voxkit-protocol Threads::Threads)
add_test(NAME voxkit-client COMMAND test_voxkit_client)

add_executable(test_voxkit_protocol test_voxkit_protocol.c)
target_link_libraries(test_voxkit_protocol PRIVATE voxkit-protocol)
add_test(NAME voxkit-protocol COMMAND test_voxkit_protocol)

add_executable(test_plus_core test_core.cpp)
target_link_libraries(test_plus_core PRIVATE chewing-plus-core)
target_include_directories(test_plus_core PRIVATE
    "${CMAKE_SOURCE_DIR}/src/plus")
target_compile_features(test_plus_core PRIVATE cxx_std_20)
target_compile_definitions(test_plus_core PRIVATE
    PLUS_EMOJI_PATH="${CMAKE_SOURCE_DIR}/data/plus-emoji-shortcodes.txt"
    PLUS_ENGLISH_PATH="${CMAKE_SOURCE_DIR}/data/plus-english.txt"
    PLUS_PHRASES_PATH="${CMAKE_SOURCE_DIR}/data/plus-phrases.txt"
    PLUS_CHARS_PATH="${CMAKE_SOURCE_DIR}/data/plus-chars.txt")

add_executable(test_plus_abi test_abi.c)
target_link_libraries(test_plus_abi PRIVATE chewing-plus-core)
# The core is C++. Link the test as C++ so libstdc++ is on the link line.
set_target_properties(test_plus_abi PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(test_plus_abi PRIVATE
    PLUS_EMOJI_PATH="${CMAKE_SOURCE_DIR}/data/plus-emoji-shortcodes.txt"
    PLUS_ENGLISH_PATH="${CMAKE_SOURCE_DIR}/data/plus-english.txt"
    PLUS_PHRASES_PATH="${CMAKE_SOURCE_DIR}/data/plus-phrases.txt"
    PLUS_CHARS_PATH="${CMAKE_SOURCE_DIR}/data/plus-chars.txt")

if(Chewing_LIBRARY_DIRS)
    set_target_properties(test_plus_core test_plus_abi PROPERTIES
        BUILD_RPATH "${Chewing_LIBRARY_DIRS}")
endif()

add_test(NAME plus-core COMMAND test_plus_core)
add_test(NAME plus-abi COMMAND test_plus_abi)

add_executable(test_model_backend test_model_backend.c)
target_link_libraries(test_model_backend PRIVATE chewing-plus-models-backend
                      Threads::Threads)
set_target_properties(test_model_backend PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED ON)
add_test(NAME model-backend COMMAND test_model_backend)

if(ENABLE_MODEL_MANAGER)
    find_program(XVFB_RUN xvfb-run)
    if(XVFB_RUN)
        add_test(NAME model-manager-smoke
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/model-manager-smoke.sh
                         $<TARGET_FILE:chewing-plus-model-manager>)
    endif()
endif()
