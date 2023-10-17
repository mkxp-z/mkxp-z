# Module for embedding resources in binaries

find_program(HEXDUMP_COMMAND NAMES xxd)

#[==[
  Adds an object library target containing embedded binary resources and
  generates a header file, where you can access them as `span<byte const>`.

  To use with older C++ standards, you can override both the `span` template
  and the `byte` type to use with e.g. span-lite, Microsoft.GSL or range-v3
  (don't forget to link your span library of choice to the generated target).

  Usage:
  add_embedded_binary_resources(
    name  # Name of the created target

    OUT_DIR <dir>  # Directory where the header and sources will be created
                   # (relative to the build directory)
    HEADER <header>  # Name of the generated header
    NAMESPACE [namespace]  # Namespace of created symbols (optional)
    RESOURCE_NAMES <names [...]>  # Names (symbols) of the resources
    RESOURCES <resources [...]>  # Resource files
    SPAN_TEMPLATE [template name]  # Name of the `span` template, default `std::span`
    SPAN_HEADER [header]  # Header with the `span` template, default `<span>`
    BYTE_TYPE [type name]  # Name of the `byte` type, default `std::byte`
    BYTE_HEADER [header]  # Header with the `byte` type, default `<cstddef>`
  )
]==]
function(add_embedded_binary_resources NAME)
  set(OPTIONS "")
  set(ONE_VALUE_ARGS OUT_DIR HEADER NAMESPACE SPAN_TEMPLATE SPAN_HEADER BYTE_TYPE BYTE_HEADER)
  set(MULTI_VALUE_ARGS RESOURCE_NAMES RESOURCES)
  cmake_parse_arguments(ARGS "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

  if(NOT HEXDUMP_COMMAND)
    message(FATAL_ERROR "Cannot embed resources - xxd not found.")
  endif()

  set(FULL_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${ARGS_HEADER}")

  if(NOT DEFINED ARGS_SPAN_TEMPLATE)
    set(ARGS_SPAN_TEMPLATE "std::span")
  endif()

  if(NOT DEFINED ARGS_SPAN_HEADER)
    set(ARGS_SPAN_HEADER "<span>")
  endif()

  if(NOT DEFINED ARGS_BYTE_TYPE)
    set(ARGS_BYTE_TYPE "std::byte")
  endif()

  if(NOT DEFINED ARGS_BYTE_HEADER)
    set(ARGS_BYTE_HEADER "<cstddef>")
  endif()

  add_library("${NAME}" OBJECT)
  target_include_directories("${NAME}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

  if(ARGS_SPAN_HEADER STREQUAL "<span>")
    target_compile_features("${NAME}" PUBLIC cxx_std_20)
  elseif(ARGS_BYTE_HEADER STREQUAL "<cstddef>")
    target_compile_features("${NAME}" PUBLIC cxx_std_17)
  endif()

  # fPIC not added automatically to object libraries due to defect in CMake
  set_target_properties(
    "${NAME}"

    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  file(
    WRITE "${FULL_HEADER_PATH}"

    "#pragma once\n"
    "\n"
    "#include ${ARGS_BYTE_HEADER}\n"
    "#include ${ARGS_SPAN_HEADER}\n"
    "\n"
  )

  if(DEFINED ARGS_NAMESPACE)
    file(
      APPEND "${FULL_HEADER_PATH}"

      "namespace ${ARGS_NAMESPACE}\n"
      "{\n"
      "\n"
    )
  endif()

  foreach(RESOURCE_NAME RESOURCE IN ZIP_LISTS ARGS_RESOURCE_NAMES ARGS_RESOURCES)
    set(FULL_RESOURCE_UNIT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${RESOURCE_NAME}.cpp")
    set(FULL_RESOURCE_HEX_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${RESOURCE_NAME}.inc")

    # Add symbol to header
    file(
      APPEND "${FULL_HEADER_PATH}"

      "[[nodiscard]] ${ARGS_SPAN_TEMPLATE}<${ARGS_BYTE_TYPE} const>\n"
      "${RESOURCE_NAME}() noexcept;\n"
      "\n"
    )

    # Write .cpp
    file(
      WRITE "${FULL_RESOURCE_UNIT_PATH}"

      "#include \"${ARGS_HEADER}\"\n"
      "\n"
      "#include <cstdint>\n"
      "\n"
    )

    if(DEFINED ARGS_NAMESPACE)
      file(
        APPEND "${FULL_RESOURCE_UNIT_PATH}"

        "namespace ${ARGS_NAMESPACE}\n"
        "{\n"
        "\n"
      )
    endif()

    file(
      APPEND "${FULL_RESOURCE_UNIT_PATH}"

      "namespace\n"
      "{\n"
      "\n"
      "std::uint8_t const ${RESOURCE_NAME}_data[] = {\n"
      "#include \"${RESOURCE_NAME}.inc\"\n"
      "};\n"
      "\n"
      "}  // namespace\n"
      "\n"
      "${ARGS_SPAN_TEMPLATE}<${ARGS_BYTE_TYPE} const>\n"
      "${RESOURCE_NAME}() noexcept\n"
      "{\n"
      "    return as_bytes(${ARGS_SPAN_TEMPLATE}<std::uint8_t const>{${RESOURCE_NAME}_data, sizeof(${RESOURCE_NAME}_data)});\n"
      "}\n"
    )

    if(DEFINED ARGS_NAMESPACE)
      file(
        APPEND "${FULL_RESOURCE_UNIT_PATH}"

        "\n"
        "}  // namespace ${ARGS_NAMESPACE}\n"
      )
    endif()

    target_sources("${NAME}" PRIVATE "${FULL_RESOURCE_UNIT_PATH}")

    add_custom_command(
      OUTPUT "${FULL_RESOURCE_HEX_PATH}"
      COMMAND "${HEXDUMP_COMMAND}" -i < "${RESOURCE}" > "${FULL_RESOURCE_HEX_PATH}"
      MAIN_DEPENDENCY "${RESOURCE}"
    )
    list(APPEND RESOURCES_HEX_FILES "${FULL_RESOURCE_HEX_PATH}")
  endforeach()

  if(DEFINED ARGS_NAMESPACE)
    file(
      APPEND "${FULL_HEADER_PATH}"

      "}  // namespace ${ARGS_NAMESPACE}\n"
    )
  endif()

  target_sources("${NAME}" PUBLIC "${FULL_HEADER_PATH}")

  add_custom_target("${NAME}_hexdump" DEPENDS "${RESOURCES_HEX_FILES}")
  add_dependencies("${NAME}" "${NAME}_hexdump")
endfunction()

#[==[
  Adds an object library target containing embedded text resources and
  generates a header file, where you can access them as `string_view`.

  To use with older C++ standards, you can override the `string_view` type
  to use with e.g. string-view-lite (don't forget to link your string view
  library of choice to the generated target).

  Usage:
  add_embedded_text_resources(
    name  # Name of the created target

    OUT_DIR <dir>  # Directory where the header and sources will be created
                   # (relative to the build directory)
    HEADER <header>  # Name of the generated header
    NAMESPACE [namespace]  # Namespace of created symbols (optional)
    RESOURCE_NAMES <names [...]>  # Names (symbols) of the resources
    RESOURCES <resources [...]>  # Resource files
    STRING_VIEW_TYPE [type name]  # Name of the `string_view` type, default `std::string_view`
    STRING_VIEW_HEADER [header]  # Header with the `string_view` type, default `<string_view>`
  )
]==]
function(add_embedded_text_resources NAME)
  set(OPTIONS "")
  set(ONE_VALUE_ARGS OUT_DIR HEADER NAMESPACE STRING_VIEW_TYPE STRING_VIEW_HEADER)
  set(MULTI_VALUE_ARGS RESOURCE_NAMES RESOURCES)
  cmake_parse_arguments(ARGS "${OPTIONS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}" ${ARGN})

  if(NOT HEXDUMP_COMMAND)
    message(FATAL_ERROR "Cannot embed resources - xxd not found.")
  endif()

  set(FULL_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${ARGS_HEADER}")

  if(NOT DEFINED ARGS_STRING_VIEW_TYPE)
    set(ARGS_STRING_VIEW_TYPE "std::string_view")
  endif()

  if(NOT DEFINED ARGS_STRING_VIEW_HEADER)
    set(ARGS_STRING_VIEW_HEADER "<string_view>")
  endif()

  add_library("${NAME}" OBJECT)
  target_include_directories("${NAME}" PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

  if(ARGS_STRING_VIEW_HEADER STREQUAL "<string_view>")
    target_compile_features("${NAME}" PUBLIC cxx_std_17)
  endif()

  # fPIC not added automatically to object libraries due to defect in CMake
  set_target_properties(
    "${NAME}"

    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  file(
    WRITE "${FULL_HEADER_PATH}"

    "#pragma once\n"
    "\n"
    "#include ${ARGS_STRING_VIEW_HEADER}\n"
    "\n"
  )

  if(DEFINED ARGS_NAMESPACE)
    file(
      APPEND "${FULL_HEADER_PATH}"

      "namespace ${ARGS_NAMESPACE}\n"
      "{\n"
      "\n"
    )
  endif()

  foreach(RESOURCE_NAME RESOURCE IN ZIP_LISTS ARGS_RESOURCE_NAMES ARGS_RESOURCES)
    set(FULL_RESOURCE_UNIT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${RESOURCE_NAME}.cpp")
    set(FULL_RESOURCE_HEX_PATH "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_OUT_DIR}/${RESOURCE_NAME}.inc")

    # Add symbol to header
    file(
      APPEND "${FULL_HEADER_PATH}"

      "[[nodiscard]] ${ARGS_STRING_VIEW_TYPE}\n"
      "${RESOURCE_NAME}() noexcept;\n"
      "\n"
    )

    # Write .cpp
    file(
      WRITE "${FULL_RESOURCE_UNIT_PATH}"

      "#include \"${ARGS_HEADER}\"\n"
      "\n"
    )

    if(DEFINED ARGS_NAMESPACE)
      file(
        APPEND "${FULL_RESOURCE_UNIT_PATH}"

        "namespace ${ARGS_NAMESPACE}\n"
        "{\n"
        "\n"
      )
    endif()

    file(
      APPEND "${FULL_RESOURCE_UNIT_PATH}"

      "namespace\n"
      "{\n"
      "\n"
      "char const ${RESOURCE_NAME}_data[] = {\n"
      "#include \"${RESOURCE_NAME}.inc\"\n"
      "};\n"
      "\n"
      "}  // namespace\n"
      "\n"
      "${ARGS_STRING_VIEW_TYPE}\n"
      "${RESOURCE_NAME}() noexcept\n"
      "{\n"
      "    return ${ARGS_STRING_VIEW_TYPE}{${RESOURCE_NAME}_data, sizeof(${RESOURCE_NAME}_data)};\n"
      "}\n"
    )

    if(DEFINED ARGS_NAMESPACE)
      file(
        APPEND "${FULL_RESOURCE_UNIT_PATH}"

        "\n"
        "}  // namespace ${ARGS_NAMESPACE}\n"
      )
    endif()

    target_sources("${NAME}" PRIVATE "${FULL_RESOURCE_UNIT_PATH}")

    add_custom_command(
      OUTPUT "${FULL_RESOURCE_HEX_PATH}"
      COMMAND "${HEXDUMP_COMMAND}" -i < "${RESOURCE}" > "${FULL_RESOURCE_HEX_PATH}"
      MAIN_DEPENDENCY "${RESOURCE}"
    )
    list(APPEND RESOURCES_HEX_FILES "${FULL_RESOURCE_HEX_PATH}")
  endforeach()

  if(DEFINED ARGS_NAMESPACE)
    file(
      APPEND "${FULL_HEADER_PATH}"

      "}  // namespace ${ARGS_NAMESPACE}\n"
    )
  endif()

  target_sources("${NAME}" PUBLIC "${FULL_HEADER_PATH}")

  add_custom_target("${NAME}_hexdump" DEPENDS "${RESOURCES_HEX_FILES}")
  add_dependencies("${NAME}" "${NAME}_hexdump")
endfunction()
