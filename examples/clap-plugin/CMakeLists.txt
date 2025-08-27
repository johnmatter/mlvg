# CLAP Plugin Example CMakeLists.txt
# Requires variables from parent project:
# - CMAKE_SOURCE_DIR (project root)
# - MADRONALIB_INCLUDE_DIR
# - MADRONALIB_LIBRARY_DIR  
# - madronalib_NAME
# - MLVG_INCLUDE_DIRS

#--------------------------------------------------------------------
# CLAP Plugin Demo Target
#--------------------------------------------------------------------

# Create embedded font resources
create_resources(${CMAKE_SOURCE_DIR}/examples/app/resources ${CMAKE_BINARY_DIR}/build/resources/clap-saw-demo)

set(target clap-saw-demo)

# Gather CLAP plugin source files
file(GLOB CLAP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB CLAP_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

list(APPEND clap_plugin_sources ${CLAP_SOURCES})
list(APPEND clap_plugin_sources ${CLAP_HEADERS})

# Add CLAP dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/source/external/clap ${CMAKE_BINARY_DIR}/external/clap EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_SOURCE_DIR}/source/external/clap-helpers ${CMAKE_BINARY_DIR}/external/clap-helpers EXCLUDE_FROM_ALL)

# Create the plugin library
add_library(${target} MODULE ${clap_plugin_sources})

# Add GUI support if enabled
option(CLAP_DEMO_GUI "Include a GUI in the CLAP demo plugin" TRUE)

if(${CLAP_DEMO_GUI})
  target_sources(${target} PRIVATE
    ${CMAKE_BINARY_DIR}/build/resources/clap-saw-demo/resources.c
  )
  
  # Set the generated resource files to compile as C++
  set_source_files_properties(${CMAKE_BINARY_DIR}/build/resources/clap-saw-demo/resources.c PROPERTIES COMPILE_FLAGS "-x c++")
  
  target_link_libraries(${target} PRIVATE mlvg)
  target_compile_definitions(${target} PRIVATE HAS_GUI=1)
endif()

# Link libraries
target_link_libraries(${target} PRIVATE clap clap-helpers)

# Add madronalib
target_include_directories(${target} PRIVATE ${MADRONALIB_INCLUDE_DIR})
target_include_directories(${target} PRIVATE ${MADRONALIB_INCLUDE_DIR}/madronalib)
if(APPLE)
  target_link_libraries(${target} PRIVATE "${MADRONALIB_LIBRARY_DIR}/lib${madronalib_NAME}.a")
elseif(WIN32)
  target_link_libraries(${target} PRIVATE "${MADRONALIB_LIBRARY_DIR}/${madronalib_NAME}.lib")
endif()

# Add include directories
target_include_directories(${target} PRIVATE
  ${CMAKE_SOURCE_DIR}/source/external/clap/include
  ${CMAKE_SOURCE_DIR}/source/external/clap-helpers/include
  ${MLVG_INCLUDE_DIRS}
)

# Platform-specific settings
if(APPLE)
  set_target_properties(${target} PROPERTIES
    BUNDLE True
    BUNDLE_EXTENSION clap
    MACOSX_BUNDLE_GUI_IDENTIFIER org.surge-synth-team.${target}
    MACOSX_BUNDLE_BUNDLE_NAME ${target}
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target}.plist.in
  )
  target_link_libraries(${target} PRIVATE "-framework CoreFoundation" "-framework AppKit" "-framework CoreGraphics")
  
  if(${CLAP_DEMO_GUI})
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK)
      target_link_libraries(${target} PRIVATE ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK})
    endif()
  endif()
  
  target_compile_definitions(${target} PRIVATE IS_MAC=1)
elseif(UNIX)
  target_compile_definitions(${target} PRIVATE IS_LINUX=1)
  set_target_properties(${target} PROPERTIES SUFFIX ".clap" PREFIX "")
else()
  target_compile_definitions(${target} PRIVATE IS_WIN=1)
  set_target_properties(${target} PROPERTIES SUFFIX ".clap" PREFIX "")
endif()

#--------------------------------------------------------------------
# Testing Infrastructure
#--------------------------------------------------------------------

# Testing infrastructure
option(CLAP_DEMO_TESTS "Enable CLAP validation testing" TRUE)

if(${CLAP_DEMO_TESTS})
  # Check if Rust/Cargo is available for clap-validator
  find_program(CARGO_EXECUTABLE cargo)
  if(CARGO_EXECUTABLE)
    message(STATUS "Found Cargo at ${CARGO_EXECUTABLE} - enabling CLAP validation tests")
    
    # Build clap-validator
    add_custom_target(build-clap-validator
      COMMAND ${CARGO_EXECUTABLE} build --release
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-validator
      COMMENT "Building clap-validator"
    )
    
    # Test target that runs clap-validator on our plugin
    add_custom_target(test-clap-plugin
      COMMAND ${CARGO_EXECUTABLE} run --release -- validate $<TARGET_FILE:${target}> --only-failed
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-validator
      DEPENDS ${target} build-clap-validator
      COMMENT "Running CLAP specification validation on ${target}"
    )
    
    # Add to CTest
    add_test(
      NAME clap-specification-compliance
      COMMAND ${CARGO_EXECUTABLE} run --release -- validate $<TARGET_FILE:${target}> --only-failed
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-validator
    )
    
    # Convenience test script targets
    if(WIN32)
      add_custom_target(test-plugin
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-plugin.bat
        COMMENT "Running full plugin test suite (Windows)"
      )
      add_custom_target(debug-plugin
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/debug-plugin.bat
        COMMENT "Running comprehensive plugin debugging suite (Windows)"
      )
    else()
      add_custom_target(test-plugin
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-plugin.sh
        COMMENT "Running full plugin test suite (Unix)"
      )
      add_custom_target(debug-plugin
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/debug-plugin.sh
        COMMENT "Running comprehensive plugin debugging suite (Unix)"
      )
    endif()
    
    # CLAP debugging tools integration
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-info/CMakeLists.txt)
      message(STATUS "Found clap-info - adding debugging targets")
      add_custom_target(build-clap-info
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-info
                ${CMAKE_COMMAND} -Bbuild -DCMAKE_BUILD_TYPE=Release
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-info/build --config Release
        COMMENT "Building clap-info debugging tool"
      )
      
      add_custom_target(inspect-plugin
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-info/build/clap-info ${CMAKE_BINARY_DIR}/${target}.clap
        DEPENDS ${target} build-clap-info
        COMMENT "Inspecting plugin with clap-info"
      )
    endif()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-host/CMakeLists.txt)
      message(STATUS "Found clap-host - adding host testing targets")
      add_custom_target(build-clap-host
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-host
                ${CMAKE_COMMAND} -Bbuild -DCMAKE_BUILD_TYPE=Release
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-host/build --config Release
        COMMENT "Building clap-host testing tool"
      )
      
      add_custom_target(test-plugin-host
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-host/build/host/clap-host -p $<TARGET_FILE:${target}>
        DEPENDS ${target} build-clap-host
        COMMENT "Testing plugin with clap-host"
      )
    endif()
  else()
    message(WARNING "Cargo not found - CLAP validation tests disabled. Install Rust from https://rustup.rs/")
  endif()
endif()
