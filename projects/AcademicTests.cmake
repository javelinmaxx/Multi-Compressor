# Recompile only the component being ablated, against the same JUCE/static
# plugin dependencies as the ordinary tests. Its object supplies those symbols
# before the static archive, so the archive's original component is not loaded.
# Class layouts are identical: experimental switches are thread-local globals.
function(j_sonic_add_ablation_tests project component)
    set(test_target "${project}AblationTests")
    add_executable(${test_target} Tests/AblationTests.cpp ${component})
    target_include_directories(${test_target} PRIVATE "${JUCE_SOURCE_DIR}/modules")
    target_compile_definitions(${test_target} PRIVATE
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 J_SONIC_ABLATION_TESTS=1)
    target_link_libraries(${test_target} PRIVATE ${project})
    add_test(NAME ${project}Ablations COMMAND ${test_target}
        "${CMAKE_CURRENT_BINARY_DIR}/academic-results")
endfunction()
