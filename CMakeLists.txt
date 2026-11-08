cmake_minimum_required(VERSION 3.20)
project(HONKR VERSION 0.2.0 LANGUAGES CXX)

# Keep windows.h from defining the min/max macros no matter which header pulls it in.
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
# The tests and tools hold engines on the stack, as Linux's and macOS's 8 MB stacks allow;
# Windows gives an executable 1 MB, so its programs get 16 MB. The plug-in keeps its engine on
# the heap and is unaffected.
if(MSVC)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " /STACK:16777216")
endif()

option(HONKR_COVERAGE "Instrument the tests for line coverage (GCC/Clang)" OFF)
option(HONKR_EDITOR "Build the native editor" ON)

function(honkr_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wno-sign-conversion>)
endfunction()
# The tests use CHECK-style soft assertions plus a few hard asserts; never strip them.
function(honkr_enable_assertions target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
endfunction()
function(honkr_coverage target)
  if(HONKR_COVERAGE AND NOT MSVC)
    target_compile_options(${target} PRIVATE --coverage -O0 -g)
    target_link_options(${target} PUBLIC --coverage)
  endif()
endfunction()

# --- third party --------------------------------------------------------------------------------
# The CLAP headers are pinned. Offline builds point FETCHCONTENT_SOURCE_DIR_CLAP at an existing
# checkout (see README).
include(FetchContent)
FetchContent_Declare(clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG 1.2.7
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(clap)

# --- the engine ---------------------------------------------------------------------------------
# Everything that makes sound, apart from the CLAP glue and the editor, so the tests can link
# it without loading the plug-in. The DSP is header-only; core.cpp compiles it once as a check.
find_package(Threads REQUIRED)
add_library(honkr_core STATIC src/core.cpp)
target_link_libraries(honkr_core PUBLIC Threads::Threads)
target_compile_features(honkr_core PUBLIC cxx_std_20)
target_include_directories(honkr_core PUBLIC src)
set_target_properties(honkr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
honkr_warnings(honkr_core)
honkr_coverage(honkr_core)

# --- the plug-in --------------------------------------------------------------------------------
if(HONKR_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(XFT REQUIRED IMPORTED_TARGET xft)
endif()

add_library(honkr_clap MODULE src/honkr.cpp)
target_compile_features(honkr_clap PRIVATE cxx_std_20)
target_include_directories(honkr_clap PRIVATE "${clap_SOURCE_DIR}/include")
target_compile_definitions(honkr_clap PRIVATE HONKR_VERSION="${PROJECT_VERSION}")
target_link_libraries(honkr_clap PRIVATE honkr_core)
honkr_warnings(honkr_clap)
honkr_coverage(honkr_clap)
if(HONKR_EDITOR)
  target_compile_definitions(honkr_clap PRIVATE HONKR_EDITOR=1)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(honkr_clap PRIVATE X11::X11 X11::Xrender PkgConfig::XFT)
  elseif(WIN32)
    target_link_libraries(honkr_clap PRIVATE user32 gdi32)
  elseif(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(src/honkr.cpp PROPERTIES LANGUAGE OBJCXX)
    target_link_libraries(honkr_clap PRIVATE "-framework Cocoa")
  endif()
endif()
set_target_properties(honkr_clap PROPERTIES
  PREFIX "" OUTPUT_NAME "HONKR" SUFFIX ".clap"
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(APPLE)
  set_target_properties(honkr_clap PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION clap SUFFIX "")
endif()
install(TARGETS honkr_clap BUNDLE DESTINATION clap LIBRARY DESTINATION clap RUNTIME DESTINATION clap)

# --- tests --------------------------------------------------------------------------------------
enable_testing()

function(honkr_test name)
  add_executable(${name} tests/${name}.cpp ${ARGN})
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE honkr_core)
  honkr_warnings(${name})
  honkr_enable_assertions(${name})
  honkr_coverage(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 900)
endfunction()

honkr_test(dsp_tests)
honkr_test(horn_tests)
honkr_test(winds_tests)
honkr_test(air_tests)
honkr_test(engine_tests)
# HONKR's distance from the reference recordings may not get worse than references/baseline.tsv.
honkr_test(reference_tests)
set_tests_properties(reference_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Suites that load the built .clap and drive it through the public CLAP interfaces, as a host does.
function(honkr_plugin_test name)
  add_executable(${name} tests/${name}.cpp)
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_include_directories(${name} PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_link_libraries(${name} PRIVATE ${CMAKE_DL_LIBS})
  honkr_warnings(${name})
  honkr_enable_assertions(${name})
  add_test(NAME ${name} COMMAND ${name} $<TARGET_FILE:honkr_clap>)
  set_tests_properties(${name} PROPERTIES TIMEOUT 600)
endfunction()
honkr_plugin_test(plugin_smoke)
honkr_plugin_test(note_tests)
honkr_plugin_test(state_tests)
# One proof per parameter, in its advertised unit, from audio. Fails if any parameter has none.
honkr_plugin_test(advertised_tests)
set_tests_properties(advertised_tests PROPERTIES TIMEOUT 1800)
target_link_libraries(advertised_tests PRIVATE honkr_core)

# The editor without a window: the real page table and widgets against a fake host.
honkr_test(ui_tests)

# Renders every preset to WAV and measures it against docs/QUALITY.md; a review tool, not a test.
add_executable(honkr-preset-audit tools/preset_audit.cpp)
target_compile_features(honkr-preset-audit PRIVATE cxx_std_20)
target_include_directories(honkr-preset-audit PRIVATE src tests)
target_link_libraries(honkr-preset-audit PRIVATE honkr_core)
honkr_warnings(honkr-preset-audit)

# HONKR against recordings of the real instruments: extracts the recordings' features and scores
# the model against them (references/README.md); a review tool. reference_tests holds the scores.
add_executable(honkr-reference tools/reference.cpp)
target_compile_features(honkr-reference PRIVATE cxx_std_20)
target_include_directories(honkr-reference PRIVATE src tests tools)
target_link_libraries(honkr-reference PRIVATE honkr_core)
honkr_warnings(honkr-reference)

# Renders single notes of an instrument straight from its model, for listening and for the
# critic; a review tool, not a test.
add_executable(honkr-note-render tools/note_render.cpp)
target_compile_features(honkr-note-render PRIVATE cxx_std_20)
target_include_directories(honkr-note-render PRIVATE src tests)
target_link_libraries(honkr-note-render PRIVATE honkr_core)
honkr_warnings(honkr-note-render)

if(HONKR_EDITOR AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # The plug-in side of the editor (host, gestures, relevance, artwork) compiled with the plug-in
  # source and driven without a window. Linux because the backend is chosen at compile time.
  add_executable(frontend_tests tests/frontend_tests.cpp)
  target_compile_features(frontend_tests PRIVATE cxx_std_20)
  target_include_directories(frontend_tests PRIVATE "${clap_SOURCE_DIR}/include" src tests)
  target_compile_definitions(frontend_tests PRIVATE HONKR_EDITOR=1 HONKR_VERSION="${PROJECT_VERSION}")
  target_link_libraries(frontend_tests PRIVATE honkr_core X11::X11 X11::Xrender PkgConfig::XFT)
  honkr_warnings(frontend_tests)
  honkr_enable_assertions(frontend_tests)
  honkr_coverage(frontend_tests)
  add_test(NAME frontend_tests COMMAND frontend_tests)
  set_tests_properties(frontend_tests PROPERTIES TIMEOUT 120)

  # Offscreen editor renders for layout review; not a test, since it needs an X display.
  add_executable(honkr-ui-preview tools/ui_preview.cpp)
  target_compile_features(honkr-ui-preview PRIVATE cxx_std_20)
  target_include_directories(honkr-ui-preview PRIVATE src)
  target_link_libraries(honkr-ui-preview PRIVATE X11::X11 X11::Xrender PkgConfig::XFT Threads::Threads)
  honkr_warnings(honkr-ui-preview)
endif()
