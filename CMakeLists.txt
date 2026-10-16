cmake_minimum_required(VERSION 3.20)
project(FUZZBOI VERSION 0.2.0 LANGUAGES CXX)

# Keep windows.h from defining the min/max macros no matter which header pulls it in.
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)

option(FUZZBOI_COVERAGE "Instrument the tests for line coverage (GCC/Clang)" OFF)
option(FUZZBOI_EDITOR "Build the native editor" ON)

function(fuzzboi_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wno-sign-conversion>)
endfunction()
# The tests use CHECK-style soft assertions plus a few hard asserts; never strip them.
function(fuzzboi_enable_assertions target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
endfunction()
function(fuzzboi_coverage target)
  if(FUZZBOI_COVERAGE AND NOT MSVC)
    target_compile_options(${target} PRIVATE --coverage -O0 -g)
    # PUBLIC: whatever links an instrumented library (the tools too) needs the gcov runtime.
    target_link_options(${target} PUBLIC --coverage)
  endif()
endfunction()

# --- third party --------------------------------------------------------------------------------
# Both dependencies are pinned. Offline builds point FETCHCONTENT_SOURCE_DIR_CLAP and
# FETCHCONTENT_SOURCE_DIR_YMFM at existing checkouts (see README).
include(FetchContent)
FetchContent_Declare(clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG 1.2.7
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(clap)
FetchContent_Declare(ymfm
  GIT_REPOSITORY https://github.com/aaronsgiles/ymfm.git
  GIT_TAG 81aec25ccbb98f4873a255f7551ac4dadac59b4a
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ymfm)

add_library(fuzzboi_ymfm STATIC
  ${ymfm_SOURCE_DIR}/src/ymfm_opm.cpp
  ${ymfm_SOURCE_DIR}/src/ymfm_misc.cpp
  ${ymfm_SOURCE_DIR}/src/ymfm_ssg.cpp)
target_include_directories(fuzzboi_ymfm SYSTEM PUBLIC ${ymfm_SOURCE_DIR}/src)
target_compile_features(fuzzboi_ymfm PUBLIC cxx_std_14)
set_target_properties(fuzzboi_ymfm PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- the engine ---------------------------------------------------------------------------------
# Everything that makes sound, apart from the CLAP glue and the editor, so the tests can link
# it without loading the plug-in.
add_library(fuzzboi_core STATIC src/dsp/fm_chip.cpp)
target_compile_features(fuzzboi_core PUBLIC cxx_std_20)
target_include_directories(fuzzboi_core PUBLIC src)
target_link_libraries(fuzzboi_core PUBLIC fuzzboi_ymfm)
set_target_properties(fuzzboi_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
fuzzboi_warnings(fuzzboi_core)
fuzzboi_coverage(fuzzboi_core)

# --- the plug-in --------------------------------------------------------------------------------
if(FUZZBOI_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XFT REQUIRED IMPORTED_TARGET xft)
endif()

add_library(fuzzboi_clap MODULE src/fuzzboi.cpp)
target_compile_features(fuzzboi_clap PRIVATE cxx_std_20)
target_include_directories(fuzzboi_clap PRIVATE "${clap_SOURCE_DIR}/include")
target_compile_definitions(fuzzboi_clap PRIVATE FUZZBOI_VERSION="${PROJECT_VERSION}")
target_link_libraries(fuzzboi_clap PRIVATE fuzzboi_core)
fuzzboi_warnings(fuzzboi_clap)
fuzzboi_coverage(fuzzboi_clap)
if(FUZZBOI_EDITOR)
  target_compile_definitions(fuzzboi_clap PRIVATE FUZZBOI_EDITOR=1)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(fuzzboi_clap PRIVATE X11::X11 X11::Xrender PkgConfig::XFT)
  elseif(WIN32)
    target_link_libraries(fuzzboi_clap PRIVATE user32 gdi32 comdlg32)
  elseif(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(src/fuzzboi.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(fuzzboi_clap PRIVATE "-framework Cocoa")
  endif()
endif()
set_target_properties(fuzzboi_clap PROPERTIES
  PREFIX "" OUTPUT_NAME "FUZZBOI" SUFFIX ".clap"
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(APPLE)
  set_target_properties(fuzzboi_clap PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap SUFFIX "")
endif()
install(TARGETS fuzzboi_clap BUNDLE DESTINATION clap LIBRARY DESTINATION clap RUNTIME DESTINATION clap)

# --- tests --------------------------------------------------------------------------------------
enable_testing()

function(fuzzboi_test name)
  add_executable(${name} tests/${name}.cpp ${ARGN})
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE fuzzboi_core)
  fuzzboi_warnings(${name})
  fuzzboi_enable_assertions(${name})
  fuzzboi_coverage(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

fuzzboi_test(dsp_tests)
fuzzboi_test(sources_tests)
fuzzboi_test(fx_tests)
fuzzboi_test(engine_tests)
fuzzboi_test(guitar_tests)

# Suites that load the built .clap and drive it through the public CLAP interfaces, as a host does.
function(fuzzboi_plugin_test name)
  add_executable(${name} tests/${name}.cpp)
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
  fuzzboi_warnings(${name})
  fuzzboi_enable_assertions(${name})
  add_test(NAME ${name} COMMAND ${name} $<TARGET_FILE:fuzzboi_clap>)
  set_tests_properties(${name} PROPERTIES TIMEOUT 300)
endfunction()
fuzzboi_plugin_test(plugin_smoke)
fuzzboi_plugin_test(note_tests)
fuzzboi_plugin_test(state_tests)
# One proof per parameter, in its advertised unit, from audio. Fails if any parameter has none.
fuzzboi_plugin_test(advertised_tests)
set_tests_properties(advertised_tests PROPERTIES TIMEOUT 900)
target_link_libraries(advertised_tests PRIVATE fuzzboi_core)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME harvest_tests COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/harvest_tests.py)
endif()

# The editor without a window: the real page table and widgets against a fake host.
fuzzboi_test(ui_tests)

# Renders every preset to WAV and measures it against docs/QUALITY.md; a review tool, not a test.
add_executable(fuzzboi-preset-audit tools/preset_audit.cpp)
target_compile_features(fuzzboi-preset-audit PRIVATE cxx_std_20)
target_include_directories(fuzzboi-preset-audit PRIVATE src tests)
target_link_libraries(fuzzboi-preset-audit PRIVATE fuzzboi_core)
fuzzboi_warnings(fuzzboi-preset-audit)

# Renders single notes of the physical guitar for tools/guitar_critic.py; a review tool, not a test.
add_executable(fuzzboi-guitar-render tools/guitar_render.cpp)
target_compile_features(fuzzboi-guitar-render PRIVATE cxx_std_20)
target_include_directories(fuzzboi-guitar-render PRIVATE src)
fuzzboi_warnings(fuzzboi-guitar-render)

if(FUZZBOI_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The plug-in side of the editor (host, gestures, relevance, artwork) compiled with the plug-in
  # source and driven without a window. Linux because the backend is chosen at compile time.
  add_executable(frontend_tests tests/frontend_tests.cpp)
  target_compile_features(frontend_tests PRIVATE cxx_std_20)
  target_include_directories(frontend_tests PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_compile_definitions(frontend_tests PRIVATE FUZZBOI_EDITOR=1 FUZZBOI_VERSION="${PROJECT_VERSION}")
  target_link_libraries(frontend_tests PRIVATE fuzzboi_core X11::X11 X11::Xrender PkgConfig::XFT)
  fuzzboi_warnings(frontend_tests)
  fuzzboi_enable_assertions(frontend_tests)
  fuzzboi_coverage(frontend_tests)
  add_test(NAME frontend_tests COMMAND frontend_tests)
  set_tests_properties(frontend_tests PROPERTIES TIMEOUT 60)

  # Offscreen editor renders for layout review; not a test, since it needs an X display.
  add_executable(fuzzboi-ui-preview tools/ui_preview.cpp)
  target_compile_features(fuzzboi-ui-preview PRIVATE cxx_std_20)
  target_include_directories(fuzzboi-ui-preview PRIVATE src)
  target_link_libraries(fuzzboi-ui-preview PRIVATE X11::X11 X11::Xrender PkgConfig::XFT)
  fuzzboi_warnings(fuzzboi-ui-preview)
endif()
