# The recorder core is compiled straight into the harness. It lives outside tests/ in code/, but
# has no engine dependencies, so the harness compiles it alone. The entry point stays here so the
# recursive glob building OpenTS cannot pick it up.
add_executable(SyncRec
    "${CMAKE_CURRENT_SOURCE_DIR}/syncrectest.cpp"
    "${CMAKE_SOURCE_DIR}/code/syncrec.cpp"
)

target_compile_features(SyncRec PRIVATE cxx_std_20)

target_include_directories(SyncRec PRIVATE
    "${CMAKE_SOURCE_DIR}/code"
)

target_compile_definitions(SyncRec PRIVATE WIN32 _WINDOWS _MBCS NOMINMAX)

target_compile_options(SyncRec PRIVATE
    $<$<CONFIG:Debug>:/MTd /EHsc /Zc:__cplusplus>
    $<$<CONFIG:Release>:/MT /EHsc /Zc:__cplusplus>
)

target_link_libraries(SyncRec PRIVATE kernel32 user32 shell32)

set_target_properties(SyncRec PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME syncrec COMMAND SyncRec)
