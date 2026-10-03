# The UDP transport is compiled directly into this asset-free harness and driven through a
# socket that reaches no network, so the send and receive passes can be exercised without
# opening a session.
add_executable(SocketUdp
    "${CMAKE_CURRENT_SOURCE_DIR}/socketudp.cpp"
    "${CMAKE_SOURCE_DIR}/code/ipxaddr.cpp"
    "${CMAKE_SOURCE_DIR}/code/netadmit.cpp"
    "${CMAKE_SOURCE_DIR}/code/netsocket_null.cpp"
    "${CMAKE_SOURCE_DIR}/code/wsproto.cpp"
    "${CMAKE_SOURCE_DIR}/code/wspudp.cpp"
)

target_compile_features(SocketUdp PRIVATE cxx_std_20)

target_include_directories(SocketUdp PRIVATE
    "${CMAKE_SOURCE_DIR}/code"
    "${OPENTS_GENERATED_DIR}"
)

add_dependencies(SocketUdp OpenTSBuildStamp)

# IPXAddressClass still reaches for ntohs when it prints an address; the transport
# itself needs no socket library here.
target_link_libraries(SocketUdp PRIVATE ws2_32)

target_compile_definitions(SocketUdp PRIVATE WIN32 _WINDOWS _MBCS NOMINMAX)

target_compile_options(SocketUdp PRIVATE
    $<$<CONFIG:Debug>:/MTd /EHsc /Zc:__cplusplus>
    $<$<CONFIG:Release>:/MT /EHsc /Zc:__cplusplus>
)

set_target_properties(SocketUdp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME socketudp COMMAND SocketUdp)
