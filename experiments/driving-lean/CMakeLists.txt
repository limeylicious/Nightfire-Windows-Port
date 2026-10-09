cmake_minimum_required(VERSION 3.20)

# ============================================================================
# YOUR_GAME_NAME - Static Recompilation
#
# Copy this template to your game project root and customize:
#   1. Change the project name below
#   2. Set XBOXRECOMP_DIR to where you cloned xboxrecomp
#   3. Adjust source file lists as you add game-specific code
#   4. Add any game-specific compile definitions or link libraries
# ============================================================================

project(nightfire_driving C)

# Separately compilable reusable backend. It is deliberately not linked into the
# bootstrap game as a whole. Live201 links only hardware.c below; the copied
# executor in this inspection target stays unlinked.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/gpu144/nightfire_hardware.c")
    add_library(driving_hardware_foundation144 OBJECT EXCLUDE_FROM_ALL
        runtime/gpu144/nightfire_hardware.c runtime/gpu144/nv2a_pb_exec.c)
    target_include_directories(driving_hardware_foundation144 PRIVATE
        runtime/gpu144 ../xboxrecomp/src ../xboxrecomp/src/d3d
        ../xboxrecomp/src/kernel ../xboxrecomp/include)
    target_compile_definitions(driving_hardware_foundation144 PRIVATE
        NIGHTFIRE_SURFACE_COHERENCE108=1 _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(driving_hardware_foundation144 PRIVATE /O2 /fp:strict /W3)
    endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    NIGHTFIRE_WORLD_RESOLVE222=1
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
#
# Option A: xboxrecomp as a subdirectory (simplest)
#   Place xboxrecomp/ next to your game project and use a relative path.
#
# Option B: find_package (if you install xboxrecomp system-wide)
#   find_package(xboxrecomp REQUIRED)
#
# Adjust this path to point to your xboxrecomp clone:
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

# Add xboxrecomp as a subdirectory -- this builds all 6 runtime libraries
# and provides the 'xboxrecomp' umbrella target to link against.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# Local adapters retain the toolkit sources and their existing notices.
get_target_property(driving_kernel_sources xbox_kernel SOURCES)
list(FILTER driving_kernel_sources EXCLUDE REGEX "(^|/)(kernel_bridge|kernel_path|kernel_file|kernel_thunks|xbox_memory_layout|nv2a_pb_scan|nv2a_pb_exec)\\.c$")
set_property(TARGET xbox_kernel PROPERTY SOURCES "${driving_kernel_sources}")
target_sources(xbox_kernel PRIVATE runtime/kernel_bridge.c runtime/kernel_path.c runtime/kernel_file.c runtime/kernel_thunks.c)
option(DRIVING_LEAN_RENDERER "Build the lean GPU-resident renderer instead of the 491 bridge" OFF)
target_sources(xbox_kernel PRIVATE runtime/xbox_memory_layout.c)
if(DRIVING_LEAN_RENDERER)
    target_sources(xbox_kernel PRIVATE runtime/lean/lean_gpu.c runtime/lean/lean_d3d.c runtime/lean/lean_sampler.c runtime/lean/lean_hang.c runtime/lean/lean_session.c)
    target_include_directories(xbox_kernel PRIVATE ${XBOXRECOMP_DIR}/src/d3d)
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_LEAN_RENDERER=1)
    target_link_libraries(xbox_kernel PRIVATE winmm)
    set_source_files_properties(runtime/lean/lean_gpu.c runtime/lean/lean_d3d.c TARGET_DIRECTORY xbox_kernel PROPERTIES COMPILE_OPTIONS "/O2")
else()
    target_sources(xbox_kernel PRIVATE runtime/driving_gpu143.c)
endif()
# Lean build only: APU output fix. The toolkit's monitor frame discarded the
# voice-processor output, so no game audio reached XAudio2. Lean copies in
# runtime/lean/apu replace those four files; ../xboxrecomp stays untouched.
if(DRIVING_LEAN_RENDERER)
    get_target_property(driving_apu_sources xbox_apu SOURCES)
    list(FILTER driving_apu_sources EXCLUDE REGEX "(^|/)(apu_core|apu_dsp|apu_vp|apu_xaudio2)\\.c$")
    set_property(TARGET xbox_apu PROPERTY SOURCES "${driving_apu_sources}")
    target_sources(xbox_apu PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/lean/apu/apu_core.c
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/lean/apu/apu_dsp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/lean/apu/apu_vp.c)
    # XAudio2 output and audio stats moved to runtime/lean/lean_audio_out.c (native audio files)
endif()
target_sources(xbox_kernel PRIVATE runtime/driving_permissions264.c)
target_sources(xbox_kernel PRIVATE runtime/driving_async295.c)
set_source_files_properties(runtime/driving_gpu143.c TARGET_DIRECTORY xbox_kernel PROPERTIES COMPILE_DEFINITIONS "DRIVING_BATCH236=1;NIGHTFIRE_BATCH_TIMING244=1;DRIVING_COMMAND245=1;DRIVING_COMMAND262=1;DRIVING_COMMAND_READ265=1;DRIVING_DRAIN_MAP274=1;NIGHTFIRE_PAIR_BATCH248=1;DRIVING_TIMING250=1;DRIVING_DESCRIPTOR252=1;DRIVING_MAPPING259=1")
if(NOT DRIVING_LEAN_RENDERER)
    target_sources(xbox_kernel PRIVATE runtime/driving_pb_exec231.c)
endif()
target_sources(xbox_kernel PRIVATE runtime/driving_dispatcher143.c)
target_sources(xbox_kernel PRIVATE runtime/driving_vblank143.c)
target_sources(xbox_kernel PRIVATE runtime/driving_launch144.c)
target_sources(xbox_kernel PRIVATE runtime/driving_ac97148.c)
target_sources(xbox_kernel PRIVATE runtime/driving_dsp149.c)
target_sources(xbox_kernel PRIVATE runtime/driving_scanout152.c)

if(MSVC)
    foreach(runtime_target xbox_kernel xbox_apu)
        target_compile_options(${runtime_target} PRIVATE
            /FI"${CMAKE_CURRENT_SOURCE_DIR}/runtime/compile_compat.h")
    endforeach()
endif()

# ── Recompiled game executable ──────────────────────────────────

# Collect generated recompiled code.
# The recomp pipeline outputs files into src/recomp/gen/.
# With --split 1000, you get gen_0000.c, gen_0001.c, etc.
# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed files
# without a manual re-configure. A plain GLOB is evaluated once at configure
# time, so a newly generated file (an extra chunk, recomp_stubs_unresolved.c)
# is silently left out of the build and shows up as unresolved externals.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

# Startup diagnostics need short rebuilds and clear original call boundaries.
# Turn this off for later performance work; it is not a gameplay-speed build.
option(DRIVING_FAST_BOOTSTRAP_BUILD "Disable generated-code optimization during engine bring-up" ON)
if(MSVC AND DRIVING_FAST_BOOTSTRAP_BUILD)
    set_source_files_properties(${RECOMP_GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/Od;/Ob0")
endif()
# Movie-only sample227 identifies the original MAD/MMX routines in this unit.
# Keep their translated instructions and strict FP semantics; compile the unit
# for execution speed so tiny packed-arithmetic helpers can be inlined.
if(MSVC)
    set_source_files_properties(src/recomp/gen/recomp_0013.c PROPERTIES COMPILE_OPTIONS "/O2;/Ob2")
endif()

# Core game sources
set(GAME_SOURCES
    src/main.c
    runtime/lean/lean_flags.c
    src/recomp_manual.c
    src/driving_port_io.c
    src/driving_memory_order.c
    src/driving_copy211.c
    src/driving_gpu_probe143.c
    src/driving_movie_probe144.c
    src/driving_audio_probe147.c
    src/driving_resource_trace156.c
    # Add game-specific sources here, e.g.:
    # src/asset_loader.c
    # src/custom_renderer.c
)

# The recomp pipeline also generates a dispatch header and function declarations.
# These are included by the generated .c files:
#   src/recomp/gen/recomp_funcs.h    - extern declarations for all recompiled functions
#   src/recomp/recomp_dispatch.h     - binary search dispatch table
#   src/recomp/recomp_types.h        - register model, ICALL macros, MEM helpers

add_executable(${PROJECT_NAME}
    ${GAME_SOURCES}
    ${RECOMP_GEN_SOURCES}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE DRIVING_CALL_HISTORY=1)

# Link against the xboxrecomp umbrella target (all 6 runtime libraries)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

# Opt-in201 bridge; only the backend translation unit, never its copied executor.
if(DRIVING_LEAN_RENDERER)
    target_sources(${PROJECT_NAME} PRIVATE runtime/driving_present201.c runtime/driving_flip204.c runtime/driving_input224.c)
    target_sources(${PROJECT_NAME} PRIVATE runtime/lean/lean_fire_test.c)  # test-only LEAN_FIRE_TEST
    target_sources(${PROJECT_NAME} PRIVATE runtime/lean/lean_dsound.c runtime/lean/lean_dsound_glue.c runtime/lean/lean_audio_out.c runtime/lean/lean_audio_dsp.c)  # LEAN_AUDIO_NATIVE
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME nightfire_driving_lean)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DRIVING_LEAN_RENDERER=1)
else()
    target_sources(${PROJECT_NAME} PRIVATE runtime/gpu144/nightfire_hardware.c runtime/driving_present201.c runtime/driving_flip204.c runtime/driving_input224.c)
endif()
target_sources(${PROJECT_NAME} PRIVATE runtime/driving_ownership309.c)
set_source_files_properties(runtime/driving_ownership309.c PROPERTIES COMPILE_OPTIONS "/O2;/fp:strict")
set_source_files_properties(runtime/gpu144/nightfire_hardware.c PROPERTIES COMPILE_DEFINITIONS "NIGHTFIRE_SURFACE_COHERENCE108=1;NIGHTFIRE_MATERIAL221=1;NIGHTFIRE_MOVIE_RGB232=1;NIGHTFIRE_PAIR234=1;NF_PAIR234_FLUSH_FIRST=1;NIGHTFIRE_BATCH234=1;NIGHTFIRE_BATCH_TIMING244=1;NIGHTFIRE_MATERIAL_RING246=1;NIGHTFIRE_PAIR_BATCH248=1;NIGHTFIRE_GPU_SAMPLE254=1;NIGHTFIRE_GPU_SAMPLE297=1" COMPILE_OPTIONS "/O2;/fp:strict")
target_link_libraries(${PROJECT_NAME} PRIVATE d3dcompiler)

# Windows SDK libraries needed at link time
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    dxguid
    xinput
    winmm
    # Lets the fault handler resolve the host RIP back to the generated
    # function's symbol, i.e. to the guest function that faulted.
    dbghelp
)

# MSVC-specific: large address aware, increase section limit for big generated files
if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
    )
    # The generated code files can be very large. Increase object file section limit.
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /fp:strict /Ob2)
endif()

# ── Game-specific additions ─────────────────────────────────────
#
# Add any game-specific libraries, loaders, or custom renderers below.
# Examples:
#
# # Custom texture loader for your game's format
# target_sources(${PROJECT_NAME} PRIVATE src/texture_loader.c)
#
# # If your game uses RenderWare, you may need a RW bridge layer
# target_sources(${PROJECT_NAME} PRIVATE src/rw_bridge.c)
#
# # Additional compile definitions for debugging
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     ENABLE_ICALL_TRACE=1
#     VERBOSE_KERNEL_LOG=1
# )


#445 optional exact VirtualQuery region cache. OFF: no source, symbol or call change.
# ON compiles the cache; it still stays inactive unless DRIVING_REGION_CACHE445=1
# (cache) or =2 (verify against the real call) is set at runtime.
option(DRIVING_REGION_CACHE445 "Build the optional generation-validated VirtualQuery region cache" OFF)
if(DRIVING_REGION_CACHE445)
    target_sources(xbox_kernel PRIVATE runtime/driving_region445.c)
    set_source_files_properties(runtime/driving_region445.c TARGET_DIRECTORY xbox_kernel PROPERTIES COMPILE_OPTIONS "/O2")
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_REGION_CACHE445=1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DRIVING_REGION_CACHE445=1)
endif()

#320 optional CPU access observer. OFF adds no memory-access branches or module.
option(DRIVING_CPU_ACCESS320 "Build bounded generated-memory observer" OFF)
option(DRIVING_ACCESS321 "Build extended span/backing diagnostic" OFF)
option(DRIVING_LEASE322 "Pin allocations only during existing target publication" OFF)
option(DRIVING_PUBLISH324 "Serialize selected kernel reads with original target publication" OFF)
option(DRIVING_COMMAND_PUBLISH325 "Serialize direct command reads with original target publication" OFF)
if(DRIVING_COMMAND_PUBLISH325)
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_COMMAND_PUBLISH325=1)
endif()
if(DRIVING_PUBLISH324 OR DRIVING_COMMAND_PUBLISH325)
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_PUBLISH324=1)
endif()
if(DRIVING_LEASE322 OR DRIVING_PUBLISH324 OR DRIVING_COMMAND_PUBLISH325)
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_LEASE322=1)
endif()
if(DRIVING_CPU_ACCESS320 OR DRIVING_ACCESS321)
    target_sources(${PROJECT_NAME} PRIVATE runtime/driving_cpu320.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DRIVING_CPU_ACCESS320=1)
    # Packet publication and allocator/watchdog hooks compile in xbox_kernel.
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_CPU_ACCESS320=1)
    set_source_files_properties(runtime/driving_cpu320.c PROPERTIES COMPILE_OPTIONS "/O2;/fp:strict")
endif()
if(DRIVING_ACCESS321)
    target_sources(${PROJECT_NAME} PRIVATE runtime/driving_access321.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DRIVING_ACCESS321=1)
    target_compile_definitions(xbox_kernel PRIVATE DRIVING_ACCESS321=1)
    set_source_files_properties(runtime/driving_access321.c PROPERTIES COMPILE_OPTIONS "/O2;/fp:strict")
endif()
