# The automatic-save bookkeeping is compiled straight into the harness. It lives in code/ but
# has no engine dependencies, so the harness compiles it alone. The entry point stays here so
# the recursive glob building OpenTS cannot pick it up.
add_executable(Autosave
    "${CMAKE_CURRENT_SOURCE_DIR}/autosavecontract.cpp"
    "${CMAKE_SOURCE_DIR}/code/autosave.cpp"
)

target_compile_features(Autosave PRIVATE cxx_std_20)

target_include_directories(Autosave PRIVATE
    "${CMAKE_SOURCE_DIR}/code"
)

target_compile_definitions(Autosave PRIVATE WIN32 _WINDOWS _MBCS NOMINMAX)

target_compile_options(Autosave PRIVATE
    $<$<CONFIG:Debug>:/MTd /EHsc /Zc:__cplusplus>
    $<$<CONFIG:Release>:/MT /EHsc /Zc:__cplusplus>
)

target_link_libraries(Autosave PRIVATE kernel32 user32 shell32)

set_target_properties(Autosave PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME autosave COMMAND Autosave)
