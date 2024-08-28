# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_header_from_json json_source_relpath)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${json_source_relpath}.h
    COMMAND ${CMAKE_COMMAND} -DJSON_SOURCE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/${json_source_relpath} -DHEADER_PATH=${CMAKE_CURRENT_BINARY_DIR}/${json_source_relpath}.h -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderFromJson.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${json_source_relpath} ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderFromJson.cmake
    VERBATIM
  )
endfunction()

function(generate_header_from_raw raw_source_relpath)
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${raw_source_relpath}.h
    COMMAND ${CMAKE_COMMAND} -DRAW_SOURCE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/${raw_source_relpath} -DHEADER_PATH=${CMAKE_CURRENT_BINARY_DIR}/${raw_source_relpath}.h -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderFromRaw.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${raw_source_relpath} ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderFromRaw.cmake
    VERBATIM
  )
endfunction()

function(generate_asmap_header)
  set(RAW_FILE "${CMAKE_SOURCE_DIR}/contrib/asmap/ip_asn.dat")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/init")
  set(GENERATED_FILE "${CMAKE_CURRENT_BINARY_DIR}/init/ip_asn.h")

  add_custom_command(
    OUTPUT ${GENERATED_FILE}
    COMMAND ${CMAKE_COMMAND} -DGENERATED_FILE=${GENERATED_FILE} -DRAW_FILE=${RAW_FILE} -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderAsmap.cmake
    DEPENDS ${RAW_FILE} ${PROJECT_SOURCE_DIR}/cmake/script/GenerateHeaderAsmap.cmake
    VERBATIM
    COMMENT "Generating embedded ASMap file init/ip_asn.h"
  )

  if(NOT ${RESULT} EQUAL 0)
    message(FATAL_ERROR "Error during init/ip_asn.h creation:\n${ERROR_OUTPUT}\n${OUTPUT}")
  endif()

  add_custom_target(generate_asmap_header ALL DEPENDS ${GENERATED_FILE})
endfunction()
