# Limit the generated solution to supported configurations.
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

# OpenTS currently supports 32-bit (x86) builds only.
if(NOT OPENTS_EXPERIMENTAL_NONWIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "OpenTS must be built as 32-bit x86. Reconfigure with -A Win32.")
    endif()
else()
    message(WARNING
        "Configuring an unsupported target. The engine assumes a 4-byte pointer and a "
        "Win32 platform layer; expect a large number of errors, and expect some of them "
        "to be silent at compile time. This configuration is for measuring, not playing.")
endif()

#
# ---------------------------------------------------------
# Directories that build as their own project
# ---------------------------------------------------------
#
set(SUBPROJECT_DIRS
    "language"
    "vqalib"
)

#
# ---------------------------------------------------------
# Gather all potential OpenTS files
# ---------------------------------------------------------
#
file(GLOB_RECURSE OPENTS_RAW CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.rc"
)

#
# ---------------------------------------------------------
# Remove files that belong to a subproject
# ---------------------------------------------------------
#
set(OPENTS_SRC "")
foreach(f ${OPENTS_RAW})
    set(EXCLUDED FALSE)
    foreach(dir ${SUBPROJECT_DIRS})
        if(f MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/${dir}/.*")
            set(EXCLUDED TRUE)
        endif()
    endforeach()

    if(NOT EXCLUDED)
        list(APPEND OPENTS_SRC "${f}")
    endif()
endforeach()

#
# ---------------------------------------------------------
# Create actual OpenTS executable (Win32)
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Creating '${PROJECT_NAME}' executable...")

add_executable(OpenTS WIN32 ${OPENTS_SRC})
target_compile_features(OpenTS PRIVATE cxx_std_20)

#
# ---------------------------------------------------------
# Output paths
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Setting binary output paths...")

set_target_properties(OpenTS PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"

    # Built under the shipped names, so the executable, the symbol file the debug record
    # names, and the map reach the run directory without a rename.
    OUTPUT_NAME_DEBUG "GameD"
    OUTPUT_NAME_RELEASE "Game"
    PDB_NAME_DEBUG "GameD"
    PDB_NAME_RELEASE "Game"
)

set_target_properties(OpenTS PROPERTIES
    VS_DEBUGGER_COMMAND "${TS_RUN_DIR}/$<TARGET_FILE_NAME:OpenTS>"
    VS_DEBUGGER_WORKING_DIRECTORY "${TS_RUN_DIR}"
)

#
# ---------------------------------------------------------
# Compile flags
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Applying compiler flags...")

if(MSVC)
    set(OPENTS_COMPILE_OPTIONS

    # ================= DEBUG =================
    $<$<CONFIG:Debug>:

        # ---------- C++ ----------
        # /arch:SSE2 -- IEEE-754 single precision, no x87 excess precision.
        # /fp:precise -- no reassociation of the engine's accumulations.
        # /utf-8 -- sources and literals are UTF-8 whatever the build machine's code page.
        $<$<COMPILE_LANGUAGE:CXX>:
            /Zi /Od /RTC1 /GR $<$<CXX_COMPILER_ID:MSVC>:/MP> /EHsc /Oy- /MTd /Zc:__cplusplus /arch:SSE2 /fp:precise /utf-8
        >

        # ---------- C ----------
        $<$<COMPILE_LANGUAGE:C>:
            /Zi /Od /RTC1 $<$<C_COMPILER_ID:MSVC>:/MP> /Oy- /arch:SSE2 /fp:precise /utf-8
        >
    >

    # ================= RELEASE =================
    $<$<CONFIG:Release>:

        # ---------- C++ ----------
        # /arch:SSE2 -- IEEE-754 single precision, no x87 excess precision.
        # /fp:precise -- no reassociation of the engine's accumulations.
        # /utf-8 -- sources and literals are UTF-8 whatever the build machine's code page.
        $<$<COMPILE_LANGUAGE:CXX>:
            /Zi /O2 /GF /GR $<$<CXX_COMPILER_ID:MSVC>:/MP> /EHsc /MT /Zc:__cplusplus /arch:SSE2 /fp:precise /utf-8
        >

        # ---------- C ----------
        $<$<COMPILE_LANGUAGE:C>:
            /Zi /O2 /GF $<$<C_COMPILER_ID:MSVC>:/MP> /arch:SSE2 /fp:precise /utf-8
        >
    >
)

target_compile_options(OpenTS PRIVATE ${OPENTS_COMPILE_OPTIONS})
else()
    # Unsupported non-Windows target. Everything the Win32 compiler gives the
    # engine for free has to be named explicitly here: the MSVC language
    # extensions the headers are written against, and the port shim, which has
    # to be parsed before any engine header so that the declarations two-phase
    # name lookup needs at template-definition time are already visible.
    # Scoped to C++: the shim is C++, and every .c in the tree is a MIDL
    # interface stub excluded from the build, so nothing in C should ever
    # receive it.
    #
    # Held in variables because VQALib needs the identical set: it is linked
    # into this executable, so it has to agree on the shim and on where
    # <windows.h> resolves.
    set(OPENTS_PORT_COMPILE_OPTIONS
        $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>
        $<$<COMPILE_LANGUAGE:CXX>:-fdeclspec>
        $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/port/port_early.h>
    )

    # The shim's own headers, and <windows.h> and friends, resolve through
    # port/shim rather than the system.
    set(OPENTS_PORT_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/port/shim"
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
    )

    target_compile_options(OpenTS PRIVATE ${OPENTS_PORT_COMPILE_OPTIONS})
    target_include_directories(OpenTS PRIVATE ${OPENTS_PORT_INCLUDE_DIRS})

    # The shim arrives through -include, so the compiler's dependency scan never
    # records port_early.h or anything it pulls in: editing the shim would rebuild
    # no object at all and leave a silently stale binary. Declare the dependency
    # explicitly so a shim change recompiles every translation unit that parses it.
    set(OPENTS_FORCE_INCLUDED_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_early.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/windows_stub.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/com_stub.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_bridge.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_gdi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_input.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_resource.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/port_windows.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/portwindow/port_window.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/port/shim/winuser.h"
    )
    foreach(f ${OPENTS_SRC})
        if(f MATCHES "\\.(cpp|cc|cxx|mm)$")
            set_property(SOURCE "${f}" APPEND PROPERTY OBJECT_DEPENDS ${OPENTS_FORCE_INCLUDED_HEADERS})
        endif()
    endforeach()
endif()

#
# ---------------------------------------------------------
# Subprojects
# ---------------------------------------------------------
#
# The VQA player carries its own project, and hands OpenTS its include path along with the
# library, so the movie code is reached the same way from anywhere in the solution.
add_subdirectory(vqalib)

if(NOT MSVC)
    # The VQA player is linked into this executable, so it has to be built
    # against the same shim the engine is and resolve <windows.h> the same way.
    # Its own project sets these for MSVC only.
    target_compile_options(VQALib PRIVATE ${OPENTS_PORT_COMPILE_OPTIONS})
    target_include_directories(VQALib PRIVATE ${OPENTS_PORT_INCLUDE_DIRS})
endif()

message(STATUS "${PROJECT_NAME}: Adding include paths...")
target_include_directories(OpenTS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OPENTS_GENERATED_DIR}"
)

# The generated build stamp has to exist before anything compiles.
add_dependencies(OpenTS OpenTSBuildStamp)

# Only the renderer's own translation unit sees bgfx, so its headers and the shaders it
# embeds are put on that file rather than on the whole target.
set(BGFX_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/bgfx.cmake/bgfx")

# bx ships per-platform compatibility headers, among them a <malloc.h> for the
# platforms that do not have one. On Apple that directory has to be on the
# include path for bx to find it -- and it has to stay off the path for
# everything else, or it shadows the system headers for every other unit.
set(BGFX_TU_INCLUDES
    "${BGFX_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/bgfx.cmake/bx/include"
    "${BGFX_ROOT}/examples/common/imgui"
)

if(APPLE)
    list(APPEND BGFX_TU_INCLUDES
        "${CMAKE_SOURCE_DIR}/thirdparty/bgfx.cmake/bx/include/compat/osx"
    )
endif()

set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/bgfxbackend.cpp" PROPERTIES
    INCLUDE_DIRECTORIES
        "${BGFX_TU_INCLUDES}"
    COMPILE_DEFINITIONS
        "BX_CONFIG_DEBUG=$<IF:$<CONFIG:Debug>,1,$<BOOL:${BX_CONFIG_DEBUG}>>"
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>"
)

# bx rewrites __stdcall while its headers are being parsed by clang-cl.  Force the
# compatibility header into the renderer translation unit as well as bx/bgfx so the
# MSVC standard-library headers that follow still see the Win32 calling convention.
if(OPENTS_EXPERIMENTAL_CLANG_CL AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/bgfxbackend.cpp" APPEND PROPERTY
        COMPILE_OPTIONS "/FI${CMAKE_SOURCE_DIR}/thirdparty/bx-clang-compat.h"
    )
endif()

message(STATUS "${PROJECT_NAME}: Adding compilier definitions...")
target_compile_definitions(OpenTS PRIVATE
    NOMINMAX

    # Compiles Blowfish into the binary instead of reaching it through the COM object in
    # blowfish.dll. It decides the layout of BlowfishEngine, so every translation unit has
    # to agree on it and it belongs on the compile line rather than in a header.
    NO_BLOWFISH_DLL
)

# WIN32 and _WINDOWS are the supported target's own macros. Defining them anywhere else
# would hide the #if !defined(_WIN32) implementations from the compiler and defeat the
# point of configuring an unsupported target.
if(WIN32)
    target_compile_definitions(OpenTS PRIVATE WIN32 _WINDOWS)
endif()

#
# ---------------------------------------------------------
# Windows libraries and linker options
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Applying linker options...")

# Required libraries:
#  Comctl32.lib
#  DbgHelp.lib
#  DSound.lib
#  Version.lib
#  Winmm.lib
#  wsock32.lib
#  + all Win32 libs
#

target_link_libraries(OpenTS PRIVATE
    VQALib

    bgfx
    bx
    bimg
    miniaudio
)

# Windows system libraries.
if(WIN32)
    target_link_libraries(OpenTS PRIVATE
        comctl32
        dbghelp
        iphlpapi
        psapi
        version
        winmm
        ws2_32
        kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32
        ole32 oleaut32 uuid odbc32 odbccp32
    )
endif()

# The vendored dependencies reach the platform themselves rather than through
# the shim, so on Apple they still need the frameworks they were written
# against. bgfx's Metal backend wants Cocoa, QuartzCore, Metal, IOKit and
# CoreVideo, its Metal video decoder wants CoreMedia and VideoToolbox, and
# miniaudio wants CoreAudio and AudioToolbox.
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    find_library(COREMEDIA_LIBRARY CoreMedia REQUIRED)
    find_library(VIDEOTOOLBOX_LIBRARY VideoToolbox REQUIRED)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)

    target_link_libraries(OpenTS PRIVATE
        ${COCOA_LIBRARY} ${QUARTZCORE_LIBRARY} ${METAL_LIBRARY} ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY} ${COREMEDIA_LIBRARY} ${VIDEOTOOLBOX_LIBRARY}
        ${COREAUDIO_LIBRARY} ${AUDIOTOOLBOX_LIBRARY} ${COREFOUNDATION_LIBRARY}
    )
endif()

# Real Cocoa window backend for the non-Windows port. Kept in its own static
# library so the single Objective-C++ translation unit is compiled WITHOUT the
# engine tree's -fms-extensions / -fdeclspec flags, which would otherwise break
# Objective-C parsing. It exposes a pure-C API (portwindow/port_window.h) that
# the C++ shim calls; the message queue lives in port_msgqueue.cpp (plain C++).
if(APPLE AND OPENTS_EXPERIMENTAL_NONWIN32)
    enable_language(OBJCXX)

    add_library(opents_portwindow STATIC "${CMAKE_CURRENT_SOURCE_DIR}/port/portwindow/port_window.mm")
    set_target_properties(opents_portwindow PROPERTIES
        OSX_ARCHITECTURES "arm64"
        CXX_STANDARD 20
    )
    # ARC is required for the __bridge_retained / __bridge_transfer ownership
    # transfers between the Cocoa window and the C void* the engine keeps as HWND.
    target_compile_options(opents_portwindow PRIVATE -fobjc-arc)
    target_include_directories(opents_portwindow PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/port")

    find_library(PW_COCOA Cocoa REQUIRED)
    find_library(PW_QUARTZ QuartzCore REQUIRED)
    find_library(PW_METAL Metal REQUIRED)
    find_library(PW_COREGRAPHICS CoreGraphics REQUIRED)
    find_library(PW_COREFOUNDATION CoreFoundation REQUIRED)
    find_library(PW_APPSERVICES ApplicationServices REQUIRED)
    target_link_libraries(opents_portwindow PRIVATE
        ${PW_COCOA} ${PW_QUARTZ} ${PW_METAL} ${PW_COREGRAPHICS} ${PW_COREFOUNDATION} ${PW_APPSERVICES})

    target_link_libraries(OpenTS PRIVATE opents_portwindow)
endif()

if(MSVC)
    target_link_options(OpenTS PRIVATE
        /SUBSYSTEM:WINDOWS
        /DEBUG
        /MAP
        # Raises the user address space from 2GB to 4GB under 64-bit Windows. Pointers may
        # then have the high bit set, so anything holding one in a signed integer must only
        # round-trip the bits and never compare or scale them.
        /LARGEADDRESSAWARE
        # Declares the process per-monitor DPI aware, so a scaled desktop does not blur
        # the frame by stretching the whole window.
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/sun.manifest"
        /MANIFEST:EMBED
    )
endif()

#
# ---------------------------------------------------------
# Visual Studio source groups
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Setting up Visual Studio filters...")

# Init Source Files (_*.cpp)
foreach(f ${OPENTS_SRC})
    if(f MATCHES "[/\\\\]_.*\\.cpp$")
        source_group("Source Files\\Init Source Files" FILES ${f})
    endif()
endforeach()

# Init Header Files (_*.h)
foreach(f ${OPENTS_SRC})
    if(f MATCHES "[/\\\\]_.*\\.h$")
        source_group("Header Files\\Init Header Files" FILES ${f})
    endif()
endforeach()

# Define Header Files (*.hh)
foreach(f ${OPENTS_SRC})
    if(f MATCHES "\\.hh$")
        source_group("Header Files\\Define Header Files" FILES ${f})
    endif()
endforeach()

# Resource Files
foreach(f ${OPENTS_SRC})
    if(f MATCHES "\\.(ico|cur|bmp|gif|jpe?g|rc)$")
        source_group("Resource Files" FILES ${f})
    endif()
endforeach()

# List of all interface filenames (headers + C stubs)
set(INTERFACE_FILES
    iblockci.h       iblockci_i.c
    iblowfish.h      iblowfish_i.c
    iflyctrl.h       iflyctrl_i.c
    ilinkstm.h
    iloco.h          iloco_i.c
    ilocos.h         ilocos_i.c
    ipiggy.h         ipiggy_i.c
    isun.h           isun_i.c
)

# Convert to full paths relative to source dir
set(FULL_INTERFACE_FILES "")
foreach(f IN LISTS INTERFACE_FILES)
    list(APPEND FULL_INTERFACE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
endforeach()

# Add them to the "Interface Files" group and exclude *_i.c from build
foreach(f IN LISTS FULL_INTERFACE_FILES)

    # Put into VS filter
    source_group("Interface Files" FILES "${f}")

    # Exclude *_i.c from build, but DO NOT mark headers as header-only
    if(f MATCHES "_i\\.c$")
        set_source_files_properties("${f}" PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()

endforeach()

# General source files
source_group("Source Files" REGULAR_EXPRESSION ".*\\.(c|cpp)$")
source_group("Header Files" REGULAR_EXPRESSION ".*\\.(h|hpp)$")

#
# ---------------------------------------------------------
# Populate the local run directory
# ---------------------------------------------------------
#
message(STATUS "${PROJECT_NAME}: Creating post-build commands...")
add_custom_command(TARGET OpenTS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${TS_RUN_DIR}"
)

add_custom_command(TARGET OpenTS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:OpenTS>"
        "${TS_RUN_DIR}"
)

# Both of the files below are produced by the Visual Studio linker. TARGET_PDB_FILE
# is an MSVC-only generator expression, so the copy cannot be expressed at all when
# the target linker is not MSVC's.
if(MSVC)
    # Copy the linker-generated .pdb alongside the exe
    add_custom_command(TARGET OpenTS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PDB_FILE:OpenTS>"
            "${TS_RUN_DIR}"
    )

    # Copy the linker-generated .map (sits next to the exe, no GenEx for it)
    add_custom_command(TARGET OpenTS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE_DIR:OpenTS>/$<TARGET_FILE_BASE_NAME:OpenTS>.map"
            "${TS_RUN_DIR}"
    )
endif()

#
# ---------------------------------------------------------
# Visual Studio startup project
# ---------------------------------------------------------
#
set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT OpenTS)

# ---------------------------------------------------------
# Add the .natvis file for Visual Studio debugging
# ---------------------------------------------------------
# /NATVIS: is an MSVC link option, and the file carries no meaning to another
# linker, so the whole block is MSVC's alone.
if(MSVC)
    set(NATVIS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/sun.natvis")
    source_group("Natvis Files" FILES ${NATVIS_FILE})
    target_sources(OpenTS PRIVATE ${NATVIS_FILE})
    target_link_options(OpenTS PRIVATE "/NATVIS:${NATVIS_FILE}")
endif()
