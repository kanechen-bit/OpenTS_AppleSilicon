# The LCW block classes and their buffer terminators are compiled straight into the harness. It
# lives outside code/ so that the recursive glob building OpenTS cannot pick this target's entry
# point up.
add_executable(LcwBlock
    "${CMAKE_CURRENT_SOURCE_DIR}/lcwblock.cpp"
    "${CMAKE_SOURCE_DIR}/code/buff.cpp"
    "${CMAKE_SOURCE_DIR}/code/lcw.cpp"
    "${CMAKE_SOURCE_DIR}/code/lcwpipe.cpp"
    "${CMAKE_SOURCE_DIR}/code/lcwstraw.cpp"
    "${CMAKE_SOURCE_DIR}/code/pipe.cpp"
    "${CMAKE_SOURCE_DIR}/code/straw.cpp"
    "${CMAKE_SOURCE_DIR}/code/xpipe.cpp"
    "${CMAKE_SOURCE_DIR}/code/xstraw.cpp"
)

target_compile_features(LcwBlock PRIVATE cxx_std_20)

target_include_directories(LcwBlock PRIVATE
    "${CMAKE_SOURCE_DIR}/code"
)

target_compile_definitions(LcwBlock PRIVATE WIN32 _WINDOWS _MBCS NOMINMAX)

# The engine builds these sources with SSE2 and precise floating point. The harness matches
# that so a result here carries over to the engine.
target_compile_options(LcwBlock PRIVATE
    $<$<CONFIG:Debug>:/MTd /EHsc /Zc:__cplusplus /arch:SSE2 /fp:precise>
    $<$<CONFIG:Release>:/MT /EHsc /Zc:__cplusplus /arch:SSE2 /fp:precise>
)

target_link_libraries(LcwBlock PRIVATE kernel32 user32 shell32)

set_target_properties(LcwBlock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME lcwblock COMMAND LcwBlock)
