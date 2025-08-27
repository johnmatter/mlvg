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
create_resources(${CMAKE_SOURCE_DIR}/examples/app/resources ${CMAKE_BINARY_DIR}/resources/clap-saw-demo)

set(target clap-saw-demo)
set(clap_target ${target}-clap)

# Gather CLAP plugin source files
file(GLOB CLAP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB CLAP_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

list(APPEND clap_plugin_sources ${CLAP_SOURCES})
list(APPEND clap_plugin_sources ${CLAP_HEADERS})

# Add CLAP dependencies
add_subdirectory(${CMAKE_SOURCE_DIR}/source/external/clap ${CMAKE_BINARY_DIR}/external/clap EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_SOURCE_DIR}/source/external/clap-helpers ${CMAKE_BINARY_DIR}/external/clap-helpers EXCLUDE_FROM_ALL)

# Create the standalone CLAP plugin library (always build this first)
add_library(${clap_target} MODULE ${clap_plugin_sources})

# Create a dummy target for VST3 wrapper (will be replaced by clap-wrapper)
add_library(${target} MODULE ${clap_plugin_sources})

# Configure both CLAP and VST3 targets
option(CLAP_DEMO_GUI "Include a GUI in the CLAP demo plugin" TRUE)

# Function to configure a CLAP plugin target
function(configure_clap_target target_name)
  if(${CLAP_DEMO_GUI})
    target_sources(${target_name} PRIVATE
      ${CMAKE_BINARY_DIR}/resources/clap-saw-demo/resources.c
    )
    
    # Set the generated resource files to compile as C++
    set_source_files_properties(${CMAKE_BINARY_DIR}/resources/clap-saw-demo/resources.c PROPERTIES COMPILE_FLAGS "-x c++")
    
    target_link_libraries(${target_name} PRIVATE mlvg)
    target_compile_definitions(${target_name} PRIVATE HAS_GUI=1)
  endif()

  # Link libraries
  target_link_libraries(${target_name} PRIVATE clap clap-helpers)

  # Add madronalib
  target_include_directories(${target_name} PRIVATE ${MADRONALIB_INCLUDE_DIR})
  target_include_directories(${target_name} PRIVATE ${MADRONALIB_INCLUDE_DIR}/madronalib)
  if(APPLE)
    target_link_libraries(${target_name} PRIVATE "${MADRONALIB_LIBRARY_DIR}/lib${madronalib_NAME}.a")
  elseif(WIN32)
    target_link_libraries(${target_name} PRIVATE "${MADRONALIB_LIBRARY_DIR}/${madronalib_NAME}.lib")
  endif()

  # Add include directories
  target_include_directories(${target_name} PRIVATE
    ${CMAKE_SOURCE_DIR}/source/external/clap/include
    ${CMAKE_SOURCE_DIR}/source/external/clap-helpers/include
    ${MLVG_INCLUDE_DIRS}
  )
endfunction()

# Configure both targets
configure_clap_target(${clap_target})
configure_clap_target(${target})

# Platform-specific settings for standalone CLAP
if(APPLE)
  set_target_properties(${clap_target} PROPERTIES
    BUNDLE True
    BUNDLE_EXTENSION clap
    MACOSX_BUNDLE_GUI_IDENTIFIER org.surge-synth-team.${target}
    MACOSX_BUNDLE_BUNDLE_NAME ${target}
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target}.plist.in
  )
  target_link_libraries(${clap_target} PRIVATE "-framework CoreFoundation" "-framework AppKit" "-framework CoreGraphics")
  
  if(${CLAP_DEMO_GUI})
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK)
      target_link_libraries(${clap_target} PRIVATE ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK})
    endif()
  endif()
  
  target_compile_definitions(${clap_target} PRIVATE IS_MAC=1)
elseif(UNIX)
  target_compile_definitions(${clap_target} PRIVATE IS_LINUX=1)
  set_target_properties(${clap_target} PROPERTIES SUFFIX ".clap" PREFIX "")
else()
  target_compile_definitions(${clap_target} PRIVATE IS_WIN=1)
  set_target_properties(${clap_target} PROPERTIES SUFFIX ".clap" PREFIX "")
endif()

# The ${target} will be configured by clap-wrapper for VST3

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
      COMMAND ${CARGO_EXECUTABLE} run --release -- validate $<TARGET_FILE:${clap_target}> --only-failed
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-validator
      DEPENDS ${clap_target} build-clap-validator
      COMMENT "Running CLAP specification validation on ${clap_target}"
    )
    
    # Add to CTest
    add_test(
      NAME clap-specification-compliance
      COMMAND ${CARGO_EXECUTABLE} run --release -- validate $<TARGET_FILE:${clap_target}> --only-failed
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
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-info/build/clap-info $<TARGET_FILE:${clap_target}>
        DEPENDS ${clap_target} build-clap-info
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
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/clap-host/build/host/clap-host -p $<TARGET_FILE:${clap_target}>
        DEPENDS ${clap_target} build-clap-host
        COMMENT "Testing plugin with clap-host"
      )
    endif()
  else()
    message(WARNING "Cargo not found - CLAP validation tests disabled. Install Rust from https://rustup.rs/")
  endif()
endif()

#--------------------------------------------------------------------
# VST3 and AUv2 Wrapper Support (using clap-wrapper from mlvg)
#--------------------------------------------------------------------

# Check if clap-wrapper functions are available
if(TARGET clap-wrapper-shared)
  message(STATUS "clap-plugin: CLAP wrapper functions available - adding VST3/AUv2 exports")
  
  # Set up VST3 SDK manually since we know the path works  
  set(VST3_SDK_ROOT "/Users/matter/Music/madrona/vst3sdk")
  
  # Create the guarantee_vst3sdk function manually (replicating base_sdks.cmake logic)
  function(guarantee_vst3sdk)
      if (TARGET base-sdk-vst3)
          return()
      endif()
      
      # Set VST3 SDK variables for our known working build
      message(STATUS "clap-wrapper: VST3 version: 3.7.14; VST3 Root ${VST3_SDK_ROOT}")
      
      # Create the base-sdk-vst3 target that wrap_vst3.cmake expects
      add_library(base-sdk-vst3 INTERFACE)
      target_include_directories(base-sdk-vst3 INTERFACE
          "${VST3_SDK_ROOT}"
          "${VST3_SDK_ROOT}/base"
          "${VST3_SDK_ROOT}/public.sdk"
          "${VST3_SDK_ROOT}/pluginterfaces"
      )
      
      # Link to the VST3 libraries we built
      target_link_libraries(base-sdk-vst3 INTERFACE
          "${VST3_SDK_ROOT}/build/lib/libbase.a"
          "${VST3_SDK_ROOT}/build/lib/libsdk.a"
          "${VST3_SDK_ROOT}/build/lib/libsdk_common.a"
          "${VST3_SDK_ROOT}/build/lib/libpluginterfaces.a"
          clap-wrapper-shared
          clap
          ghc_filesystem
      )
  endfunction()
  
  # Create the macos_include_clap_in_bundle function manually (from shared_prologue.cmake)
  function(macos_include_clap_in_bundle)
      set(oneValueArgs
              TARGET
              MACOS_EMBEDDED_CLAP_LOCATION
              MACOSX_EMBEDDED_CLAP_LOCATION
      )
      
      cmake_parse_arguments(MBC "" "${oneValueArgs}" "" ${ARGN})
      
      if (NOT DEFINED MBC_MACOS_EMBEDDED_CLAP_LOCATION AND DEFINED MBC_MACOSX_EMBEDDED_CLAP_LOCATION)
          set(MBC_MACOS_EMBEDDED_CLAP_LOCATION ${MBC_MACOSX_EMBEDDED_CLAP_LOCATION})
      endif()
      
      if (NOT DEFINED MBC_MACOSX_EMBEDDED_CLAP_LOCATION AND DEFINED MBC_MACOS_EMBEDDED_CLAP_LOCATION)
          set(MBC_MACOSX_EMBEDDED_CLAP_LOCATION ${MBC_MACOS_EMBEDDED_CLAP_LOCATION})
      endif()
      
      if (NOT APPLE)
          message(WARNING "Calling macos_include_clap_in_bundle on non APPLE system. Is this intentional?")
          return()
      endif()
      
      # Basic implementation - this copies the CLAP into the VST3 bundle
      if (MBC_MACOS_EMBEDDED_CLAP_LOCATION)
          add_custom_command(TARGET ${MBC_TARGET} POST_BUILD 
              COMMAND ${CMAKE_COMMAND} -E make_directory 
                  "$<TARGET_BUNDLE_DIR:${MBC_TARGET}>/Contents/${MBC_MACOS_EMBEDDED_CLAP_LOCATION}"
              COMMAND ${CMAKE_COMMAND} -E copy_directory 
                  "$<TARGET_BUNDLE_DIR:${clap_target}>"
                  "$<TARGET_BUNDLE_DIR:${MBC_TARGET}>/Contents/${MBC_MACOS_EMBEDDED_CLAP_LOCATION}/${clap_target}.clap"
              DEPENDS ${clap_target}
          )
      endif()
  endfunction()
  
  # Create the macos_bundle_flag function manually (from shared_prologue.cmake)
  function(macos_bundle_flag)
      set(oneValueArgs TARGET)
      cmake_parse_arguments(MBF "" "${oneValueArgs}" "" ${ARGN})
      
      if (NOT APPLE)
          message(WARNING "Calling macos_bundle_flag on non APPLE system. Is this intentional?")
          return()
      endif()
      
      if (NOT ${CMAKE_GENERATOR} STREQUAL "Xcode")
          add_custom_command(TARGET ${MBF_TARGET} POST_BUILD
              COMMAND /usr/bin/plutil -replace CFBundleExecutable -string ${MBF_TARGET} "$<TARGET_BUNDLE_DIR:${MBF_TARGET}>/Contents/Info.plist"
              COMMAND /usr/bin/plutil -replace CFBundleVersion -string "1.0.0" "$<TARGET_BUNDLE_DIR:${MBF_TARGET}>/Contents/Info.plist"
              COMMAND /usr/bin/plutil -replace CFBundleShortVersionString -string "1.0.0" "$<TARGET_BUNDLE_DIR:${MBF_TARGET}>/Contents/Info.plist"
          )
      endif()
  endfunction()
  
  # Now include VST3 wrapper
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external/clap-wrapper/cmake")
  include(${CMAKE_SOURCE_DIR}/external/clap-wrapper/cmake/wrap_vst3.cmake)
  
  # VST3 Wrapper
  option(CLAP_DEMO_BUILD_VST3 "Build VST3 wrapper for CLAP plugin demo" ON)
  if(CLAP_DEMO_BUILD_VST3)
    target_add_vst3_wrapper(
      TARGET ${target}
      OUTPUT_NAME "${target}"
      BUNDLE_IDENTIFIER "com.madronalabs.${target}.vst3"
      BUNDLE_VERSION "1.0.0"
    )
    
    # Add required include paths to the VST3 wrapper target
    target_include_directories(${target}-clap-wrapper-vst3-lib PRIVATE
        ${CMAKE_SOURCE_DIR}/external/clap-wrapper/libs/fmt
        ${CMAKE_SOURCE_DIR}/source/external/filesystem/include
        ${CMAKE_SOURCE_DIR}/external/clap-wrapper/src
        ${CMAKE_SOURCE_DIR}/external/clap-wrapper/include
    )
    
    # Add required compiler definitions for VST3 SDK and clap-wrapper
    target_compile_definitions(${target}-clap-wrapper-vst3-lib PRIVATE
        RELEASE=1  # Required by VST3 SDK
        CLAP_WRAPPER_HAS_GHCFS=1  # Use ghc::filesystem instead of std::filesystem
    )
    
    # Set macOS deployment target for filesystem support
    if(APPLE)
        target_compile_options(${target}-clap-wrapper-vst3-lib PRIVATE 
            -mmacosx-version-min=10.15
        )
        
        # Ensure VST3 wrapper uses the same architecture as the main build
        if(CMAKE_OSX_ARCHITECTURES)
            set_target_properties(${target}-clap-wrapper-vst3-lib PROPERTIES 
                OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
            )
            # Also set for the final VST3 bundle target
            set_target_properties(${target} PROPERTIES 
                OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
            )
            
            message(STATUS "clap-plugin: Set VST3 wrapper architecture to ${CMAKE_OSX_ARCHITECTURES}")
        endif()
    endif()
    
    # Add direct links to the final VST3 bundle target to ensure all libraries are linked
    target_link_libraries(${target} PRIVATE 
        "${VST3_SDK_ROOT}/build/lib/libsdk.a"
        "${VST3_SDK_ROOT}/build/lib/libsdk_common.a"
        "${VST3_SDK_ROOT}/build/lib/libbase.a"
        "${VST3_SDK_ROOT}/build/lib/libpluginterfaces.a"
        clap
        ghc_filesystem
        clap-wrapper-shared
    )
    
    # Add missing frameworks for VST3 target (same as CLAP)
    if(APPLE AND ${CLAP_DEMO_GUI})
        target_link_libraries(${target} PRIVATE 
            "-framework CoreFoundation" "-framework AppKit" "-framework CoreGraphics"
        )
        find_library(METAL_FRAMEWORK Metal)
        find_library(METALKIT_FRAMEWORK MetalKit)
        if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK)
            target_link_libraries(${target} PRIVATE ${METAL_FRAMEWORK} ${METALKIT_FRAMEWORK})
        endif()
    endif()
    
    # Also ensure the VST3 wrapper library itself is properly linked
    target_link_libraries(${target}-clap-wrapper-vst3-lib PRIVATE 
        "${VST3_SDK_ROOT}/build/lib/libsdk.a"
        "${VST3_SDK_ROOT}/build/lib/libsdk_common.a"
        "${VST3_SDK_ROOT}/build/lib/libbase.a"
        "${VST3_SDK_ROOT}/build/lib/libpluginterfaces.a"
        clap
        ghc_filesystem
        clap-wrapper-shared
    )
    
    # Add VST3 SingleComponentEffect source directly to wrapper library
    target_sources(${target}-clap-wrapper-vst3-lib PRIVATE
        "${VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
    )
    
    # Embed the CLAP plugin in the VST3 bundle for thick wrapper approach
    if(APPLE)
        macos_include_clap_in_bundle(
            TARGET ${target}
            MACOS_EMBEDDED_CLAP_LOCATION "clap"
        )
    endif()
    
    # Post-build step to fix VST3 bundle attributes  
    if(APPLE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND /usr/bin/xattr -c "${CMAKE_CURRENT_BINARY_DIR}/${target}.vst3" || true
            COMMAND printf "BNDL????" > "${CMAKE_CURRENT_BINARY_DIR}/${target}.vst3/Contents/PkgInfo"
            COMMAND /usr/bin/codesign --force --sign - --preserve-metadata=identifier,entitlements,flags --timestamp=none "${CMAKE_CURRENT_BINARY_DIR}/${target}.vst3"
            COMMAND /usr/bin/SetFile -a B "${CMAKE_CURRENT_BINARY_DIR}/${target}.vst3"
            COMMENT "Creating PkgInfo, signing, and setting bundle bit for VST3 plugin"
            VERBATIM
        )
    endif()
    
    message(STATUS "clap-plugin: VST3 wrapper target added")
  endif()
  
  # AUv2 Wrapper (macOS only)
  if(APPLE)
    option(CLAP_DEMO_BUILD_AUV2 "Build AUv2 wrapper for CLAP plugin demo" OFF)
    if(CLAP_DEMO_BUILD_AUV2)
      target_add_auv2_wrapper(
        TARGET ${target}
        OUTPUT_NAME "${target}"
        BUNDLE_IDENTIFIER "com.madronalabs.${target}.component"
        BUNDLE_VERSION "1.0.0"
        MANUFACTURER_NAME "Madrona Labs"
        MANUFACTURER_CODE "MdLb"
        SUBTYPE_CODE "CSDm"  # Clap Saw Demo
        INSTRUMENT_TYPE "aumu"  # Music Device (instrument)
      )
      message(STATUS "clap-plugin: AUv2 wrapper target added")
    endif()
  endif()
  
else()
  message(STATUS "clap-plugin: CLAP wrapper not available - only building CLAP format")
endif()
