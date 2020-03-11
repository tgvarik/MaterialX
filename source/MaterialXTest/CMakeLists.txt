cmake_policy(SET CMP0079 NEW)

set(MATERIALX_TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Discover all tests and allow them to be run in parallel (ctest -j20):
function(add_tests _sources)
  foreach(src_file ${_sources})
    file(STRINGS ${src_file} matched_lines REGEX "TEST_CASE")
    foreach(matched_line ${matched_lines})
      string(REGEX REPLACE "(TEST_CASE[( \"]+)" "" test_name ${matched_line})
      string(REGEX REPLACE "(\".*)" "" test_name ${test_name})
      string(REGEX REPLACE "[^A-Za-z0-9_]+" "_" test_safe_name ${test_name})
      add_test(NAME "MaterialXTest_${test_safe_name}"
          COMMAND MaterialXTest ${test_name}
          WORKING_DIRECTORY ${MATERIALX_TEST_BINARY_DIR})
    endforeach()
  endforeach()
endfunction()

add_executable(MaterialXTest)

target_include_directories( MaterialXTest PUBLIC
    ${EXTERNAL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../)

file(GLOB materialx_source "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB materialx_headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

list(APPEND materialx_headers "${CMAKE_CURRENT_SOURCE_DIR}/Catch/catch.hpp")

assign_source_group("Source Files" ${materialx_source})
assign_source_group("Header Files" ${materialx_headers})

target_sources(MaterialXTest PUBLIC ${materialx_source} ${materialx_headers})

add_subdirectory(MaterialXCore)
add_subdirectory(MaterialXFormat)

if(MATERIALX_BUILD_GEN)
  add_subdirectory(MaterialXGenShader)
  if(MATERIALX_BUILD_RUNTIME)
    add_subdirectory(MaterialXRuntime)
  endif()
  if(MATERIALX_BUILD_GEN_GLSL)
    add_subdirectory(MaterialXGenGlsl)
  endif()
  if(MATERIALX_BUILD_GEN_OSL)
    add_subdirectory(MaterialXGenOsl)
  endif()
  if(MATERIALX_BUILD_GEN_OGSXML)
    add_subdirectory(MaterialXGenOgsXml)
  endif()
  if(MATERIALX_BUILD_GEN_OGSFX)
    add_subdirectory(MaterialXGenOgsFx)
  endif()
  if(MATERIALX_BUILD_GEN_ARNOLD)
    add_subdirectory(MaterialXGenArnold)
  endif()
endif()

if(MATERIALX_BUILD_RENDER)
  add_subdirectory(MaterialXRender)
  if(MATERIALX_BUILD_GEN_GLSL)
    add_subdirectory(MaterialXRenderGlsl)
  endif()
  if(MATERIALX_BUILD_GEN_OSL)
    add_subdirectory(MaterialXRenderOsl)
  endif()
  if(MATERIALX_BUILD_GEN_OGSFX)
    add_subdirectory(MaterialXRenderOgsFx)
  endif()
  if(MATERIALX_BUILD_GEN_ARNOLD)
    add_subdirectory(MaterialXRenderArnold)
  endif()
  if(MATERIALX_BUILD_OIIO AND OPENIMAGEIO_ROOT_DIR)
      add_custom_command(TARGET MaterialXTest POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${OPENIMAGEIO_ROOT_DIR}/bin ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()

add_custom_command(TARGET MaterialXTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/../../libraries ${CMAKE_CURRENT_BINARY_DIR}/libraries)

install(DIRECTORY DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/libraries/stdlib/reference/osl)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/stdlib/osl/"
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/libraries/stdlib/reference/osl 
    FILES_MATCHING PATTERN "*.h")

add_custom_command(TARGET MaterialXTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/../../resources ${CMAKE_CURRENT_BINARY_DIR}/resources)

set_target_properties(
    MaterialXTest PROPERTIES
    OUTPUT_NAME MaterialXTest
    COMPILE_FLAGS "${EXTERNAL_COMPILE_FLAGS}"
    LINK_FLAGS "${EXTERNAL_LINK_FLAGS}"
    VERSION "${MATERIALX_LIBRARY_VERSION}"
    SOVERSION "${MATERIALX_MAJOR_VERSION}"
    DEBUG_POSTFIX "${MATERIALX_DEBUG_POSTFIX}")

#set(LIBS
#    MaterialXCore
#    MaterialXFormat
#    MaterialXGenShader
#    MaterialXGenOsl
#    MaterialXGenGlsl
#    MaterialXRender
#    MaterialXRenderOsl
#    MaterialXRenderHw
#    MaterialXRenderGlsl
#)
#
## Add language specific generators
#if (MATERIALX_BUILD_GEN_ARNOLD)
#    LIST(APPEND LIBS "MaterialXGenArnold")
#endif()
#if (MATERIALX_BUILD_GEN_OGSFX)
#    LIST(APPEND LIBS "MaterialXGenOgsFx")
#endif()
#if (MATERIALX_BUILD_GEN_OGSXML)
#    LIST(APPEND LIBS "MaterialXGenOgsXml")
#endif()
#
## Add contrib libraries
#if (MATERIALX_BUILD_CONTRIB)
#    LIST(APPEND LIBS "MaterialXContrib")
#endif()
#
## Add runtime library
#if (MATERIALX_BUILD_RUNTIME)
#    LIST(APPEND LIBS "MaterialXRuntime")
#endif()

target_link_libraries(
    MaterialXTest
#    ${LIBS}
    ${CMAKE_DL_LIBS})
