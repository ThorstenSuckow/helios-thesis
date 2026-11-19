cmake_minimum_required(VERSION 4.0)
project(helios-thesis-scenarios LANGUAGES C CXX)

include(FetchContent)

# This project lives in <workspace>/helios-thesis/thesis-scenarios; the helios-* modules
# are siblings of <workspace>/helios-thesis (override with -DHELIOS_WORKSPACE_DIR=...).
get_filename_component(_HELIOS_WORKSPACE_DEFAULT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(HELIOS_WORKSPACE_DIR "${_HELIOS_WORKSPACE_DEFAULT}" CACHE PATH "Directory containing the helios-* modules")

set(
    HELIOS_USE_DEVTOOLS
    OFF
    CACHE BOOL
    "Load shared tooling from helios-devtools"
    FORCE
)

set(HELIOS_DEVTOOLS_GIT_REPOSITORY "https://github.com/thorstensuckow/helios-devtools.git" CACHE STRING "helios-devtools git repository")
set(HELIOS_DEVTOOLS_GIT_TAG "v1.0.0" CACHE STRING "helios-devtools git tag or commit")
set(HELIOS_DEVTOOLS_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-devtools" CACHE PATH "Optional local path to helios-devtools")

if(HELIOS_USE_DEVTOOLS)
    if(HELIOS_DEVTOOLS_LOCAL_PATH)
        FetchContent_Declare(
                helios_devtools
                SOURCE_DIR "${HELIOS_DEVTOOLS_LOCAL_PATH}"
        )
    else()
        FetchContent_Declare(
                helios_devtools
                GIT_REPOSITORY "${HELIOS_DEVTOOLS_GIT_REPOSITORY}"
                GIT_TAG "${HELIOS_DEVTOOLS_GIT_TAG}"
        )
    endif()

    FetchContent_MakeAvailable(helios_devtools)
    include("${helios_devtools_SOURCE_DIR}/cmake/HeliosDevtools.cmake")
endif()

function(helios_enable_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
                /W4
                /we4715
        )
    else()
        target_compile_options(${target} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wreturn-type
                -Werror=return-type
        )
    endif()
endfunction()

# Compile-time scheduler measurement (bit mask, see helios-ecs-config.h):
#   0 = off, 1 = schedules, 2 = systems/managers (update + flush), 3 = both
# thesis scenarios read schedule, runtime system and parallel system group timings -> always 7 (forced, overrides stale caches).
set(HELIOS_PERFORMANCE 7 CACHE STRING "Scheduler measurement bit mask (1 = schedules, 2 = systems/managers, 4 = parallel system groups, 7 = all)" FORCE)
add_compile_definitions(HELIOS_PERFORMANCE=${HELIOS_PERFORMANCE})

# A/B build-time tests: ON makes the ConflictFree* concepts skip the static conflict analysis.
option(HELIOS_CONFLICT_ANALYSIS_OFF "Disable static conflict analysis (ConflictFree* concepts)" OFF)
if(HELIOS_CONFLICT_ANALYSIS_OFF)
    add_compile_definitions(HELIOS_CONFLICT_ANALYSIS_OFF)
endif()

set(HELIOS_ECS_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-ecs" CACHE PATH "Local path to helios-ecs")
set(HELIOS_ENGINE_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-engine" CACHE PATH "Local path to helios-engine")
set(HELIOS_OPENGL_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-opengl" CACHE PATH "Local path to helios-opengl")
set(HELIOS_GLFW_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-glfw" CACHE PATH "Local path to helios-glfw")
set(HELIOS_IMGUI_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-imgui" CACHE PATH "Local path to helios-imgui")
set(HELIOS_PHYSICS_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-physics" CACHE PATH "Local path to helios-physics")
set(HELIOS_GAMEPLAY_LOCAL_PATH "${HELIOS_WORKSPACE_DIR}/helios-gameplay" CACHE PATH "Local path to helios-gameplay")

if(NOT TARGET helios::ecs)
    if(EXISTS "${HELIOS_ECS_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_ECS_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-ecs-build")
    else()
        FetchContent_Declare(helios_ecs
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-ecs.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_ecs)
    endif()
endif()


if(NOT TARGET helios::engine)
    if(EXISTS "${HELIOS_ENGINE_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_ENGINE_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-engine-build")
    else()
        FetchContent_Declare(helios_engine
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-engine.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_engine)
    endif()
endif()

if(NOT TARGET helios::opengl)
    if(EXISTS "${HELIOS_OPENGL_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_OPENGL_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-opengl-build")
    else()
        FetchContent_Declare(helios_opengl
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-opengl.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_opengl)
    endif()
endif()

if(NOT TARGET helios::glfw)
    if(EXISTS "${HELIOS_GLFW_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_GLFW_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-glfw-build")
    else()
        FetchContent_Declare(helios_glfw
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-glfw.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_glfw)
    endif()
endif()

if(NOT TARGET helios::gameplay)
    if(EXISTS "${HELIOS_GAMEPLAY_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_GAMEPLAY_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-gameplay-build")
    else()
        FetchContent_Declare(helios_gameplay
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-gameplay.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_gameplay)
    endif()
endif()

if(NOT TARGET helios::imgui)
    if(EXISTS "${HELIOS_IMGUI_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_IMGUI_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-imgui-build")
    else()
        FetchContent_Declare(helios_imgui
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-imgui.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_imgui)
    endif()
endif()

if(NOT TARGET helios::physics)
    if(EXISTS "${HELIOS_PHYSICS_LOCAL_PATH}/CMakeLists.txt")
        add_subdirectory("${HELIOS_PHYSICS_LOCAL_PATH}" "${CMAKE_BINARY_DIR}/_deps/helios-physics-build")
    else()
        FetchContent_Declare(helios_physics
                GIT_REPOSITORY https://github.com/thorstensuckow/helios-physics.git
                GIT_TAG master-thesis
        )
        FetchContent_MakeAvailable(helios_physics)
    endif()
endif()

# Layout:
#   src/lib/                     shared modules (thesis.config, thesis.util, ...), include/ and resources/
#   src/v<k>/src/v<k>_main.cpp   one executable per scenario, plus its local HandleSetup.ixx
set(SCENARIOS_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/lib")

file(GLOB_RECURSE SCENARIO_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/v*/src/*.cpp")

# Standalone diagnostics (src/diag/*.cpp): no shared thesis modules, no bootstrap / thesis.handles.
file(GLOB_RECURSE DIAG_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/diag/*.cpp")
list(APPEND SCENARIO_FILES ${DIAG_FILES})

file(GLOB_RECURSE SHARED_MODULES CONFIGURE_DEPENDS
        "${SCENARIOS_LIB_DIR}/*.ixx")

foreach(SCENARIO_FILE ${SCENARIO_FILES})
    file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${SCENARIO_FILE})
    get_filename_component(SUBDIR ${REL_PATH} DIRECTORY)
    get_filename_component(TARGET_NAME ${SCENARIO_FILE} NAME_WE)

    if(TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Duplicate scenario target name '${TARGET_NAME}' (${REL_PATH})")
    endif()

    add_executable(${TARGET_NAME} ${SCENARIO_FILE})

    helios_enable_strict_warnings(${TARGET_NAME})

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /bigobj)
    endif()

    target_link_libraries(${TARGET_NAME} PRIVATE
            helios::ecs
            helios::engine
            helios::opengl
            helios::glfw
            helios::imgui
            helios::gameplay
            helios::physics
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE GLFW_INCLUDE_NONE)

    get_filename_component(SCENARIO_DIR ${SCENARIO_FILE} DIRECTORY)

    # scenario-local modules (e.g. HandleSetup.ixx providing thesis.handles)
    file(GLOB_RECURSE SCENARIO_MODULES CONFIGURE_DEPENDS
            "${SCENARIO_DIR}/*.ixx"
    )

    # The shared lib/bootstrap.ixx imports the scenario-local module
    # `thesis.handles`; only use it for scenarios providing a HandleSetup.ixx.
    # Diagnostics (src/diag) are standalone and get no shared modules at all.
    set(SHARED_MODULES_FOR_TARGET ${SHARED_MODULES})
    if(SCENARIO_FILE IN_LIST DIAG_FILES)
        set(SHARED_MODULES_FOR_TARGET "")
    elseif(NOT EXISTS "${SCENARIO_DIR}/HandleSetup.ixx")
        list(FILTER SHARED_MODULES_FOR_TARGET EXCLUDE REGEX "/lib/bootstrap\\.ixx$")
    endif()

    set(TARGET_MODULE_FILES ${SHARED_MODULES_FOR_TARGET} ${SCENARIO_MODULES})
    if(TARGET_MODULE_FILES)
        target_sources(${TARGET_NAME}
                PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES FILES
                ${TARGET_MODULE_FILES}
        )
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/scenarios/${SUBDIR}
            OUTPUT_NAME ${TARGET_NAME}
    )

    set(RESOURCES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/resources")
    set(RESOURCES_DEST_DIR "${CMAKE_BINARY_DIR}/scenarios/${SUBDIR}/resources")

    # Shared resources (e.g. the common shader files) are copied next to every
    # scenario binary first; a scenario-local resources folder (if present) is
    # copied afterwards and can override/add to it.
    if(EXISTS "${SCENARIOS_LIB_DIR}/resources")
        add_custom_command(
                TARGET ${TARGET_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SCENARIOS_LIB_DIR}/resources"
                "${RESOURCES_DEST_DIR}"
        )
    endif()

    if(EXISTS "${RESOURCES_SOURCE_DIR}")
        add_custom_command(
                TARGET ${TARGET_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RESOURCES_SOURCE_DIR}"
                "${RESOURCES_DEST_DIR}"
        )
    endif()
endforeach()

