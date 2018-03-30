macro(libpgm_say message_to_user)
  message(STATUS "LIBPGM: ${message_to_user}")
endmacro()

# void libpgm_warning_say(string message_to_user);
# - prints out message_to_user with a warning
macro(libpgm_warning_say message_to_user)
  message(WARNING "LIBPGM: ${message_to_user}")
endmacro()

# void libpgm_error_say(string message_to_user);
# - prints out message_to_user with an error and exits cmake
macro(libpgm_error_say message_to_user)
  message(FATAL_ERROR "LIBPGM: ${message_to_user}")
endmacro()

macro(get_current_name name)
  get_filename_component(${name} ${CMAKE_CURRENT_SOURCE_DIR} NAME)
endmacro()

macro(get_parent_name name)
  set(path "")
  get_filename_component(path ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
  get_filename_component(${name} ${path} NAME)
endmacro()

function(removeDuplicateSubstring out in)
    separate_arguments(in)
    list(REMOVE_DUPLICATES in)
    string(REPLACE ";" " " in "${in}")
    set(${out} "${in}" PARENT_SCOPE)
endfunction()

function(libmath_add_object_library SRCS FLAGS DEFINITIONS LIBNAME)
  set(NAME)
  if(LIBNAME STREQUAL "")
    string(REPLACE "/lib/" ";" NEWPATH ${CMAKE_CURRENT_SOURCE_DIR})
    list (GET NEWPATH 1 NAME)
    string(REPLACE "/" "_" NAME ${NAME})
  else()
    set(NAME "${LIBNAME}")
  endif()
  # message(STATUS "Configuring object library: " ${NAME})

  # Add object library name to global target object for static library creation
  set_property(GLOBAL APPEND PROPERTY "TARGET_OBJECTS" $<TARGET_OBJECTS:${NAME}>)

  # Set the -I includes for all sources
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  # Add object library, a collection of object files
  add_library(${NAME} OBJECT ${SRCS})

  # Set the compiler flags and definitions for the target
  set_property(TARGET ${NAME} APPEND PROPERTY COMPILE_FLAGS "${FLAGS}")
  set_property(TARGET ${NAME} APPEND PROPERTY COMPILE_DEFINITIONS ${DEFINITIONS})
endfunction()

function(libmath_add_object_library_asm NAME FLAGS DEFINITIONS)
  set(TARGET_NAME ${NAME})
  list(APPEND PREPROCESSOR "${CMAKE_C_COMPILER} -S ${DEFINITIONS} ${FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}.cpp")
  separate_arguments(PREPROCESSOR UNIX_COMMAND "${PREPROCESSOR}")

  add_custom_command(OUTPUT ${TARGET_NAME}.s
    COMMAND ${PREPROCESSOR}
    DEPENDS "${TARGET_NAME}.cpp")

  add_custom_command(OUTPUT ${TARGET_NAME}_awk.s
    COMMAND sh "${LIBPGM_TOOLS_DIR}/awk_asm.sh" ${TARGET_NAME}.s ${TARGET_NAME}_awk.s
    DEPENDS "${TARGET_NAME}.s")

  add_custom_target(${TARGET_NAME} ALL DEPENDS "${TARGET_NAME}_awk.s")

  libmath_add_object_library("${TARGET_NAME}_awk.s" "${FLAGS}" "${DEFINITIONS}" "${TARGET_NAME}_build")
  add_dependencies("${TARGET_NAME}_build" ${TARGET_NAME})
endfunction()
