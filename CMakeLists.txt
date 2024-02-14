cmake_minimum_required(VERSION 3.16)

set(EXE_TARGET "jdepp")
set(PY_TARGET "jdepp_ext")
project(${EXE_TARGET} CXX)

option(JDEPP_WITH_PYTHON "Build Python module(For developer)." Off)
option(
    JDEPP_PREFER_LOCAL_PYTHON_INSTALLATION
    "Prefer locally-installed Python interpreter than system or conda/brew installed Python. Please specify your Python interpreter with `Python3_EXECUTABLE` cmake option if you enable this option."
    OFF)


# cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers) # Address sanitizer (-DSANITIZE_ADDRESS=ON)

# Build standalone .so(for developer)
if (JDEPP_WITH_PYTHON)

  if(JDEPP_PREFER_LOCAL_PYTHON_INSTALLATION)
    #message(STATUS "Local Python")
    set(Python3_FIND_FRAMEWORK NEVER) # Do not search framework python
    set(Python3_FIND_STRATEGY LOCATION)
    set(Python3_FIND_REGISTRY NEVER) # Windows only
  else()
    set(Python3_FIND_FRAMEWORK LAST
    )# Prefer Brew/Conda to Apple framework python
  endif()

  find_package(
    Python3
    COMPONENTS Interpreter Development
    REQUIRED)

  find_package(pybind11 CONFIG)

endif()


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JDEPP_SOURCES
  jdepp/classify.cc
  jdepp/classify.h
  jdepp/kernel.cc
  jdepp/kernel.h
  jdepp/linear.cc
  jdepp/linear.h
  jdepp/pdep.cc
  jdepp/pdep.h
  jdepp/pecco.h
  jdepp/pa.h
  jdepp/cedar.h
  jdepp/io-util.cc
  jdepp/io-util.hh
  )

add_executable(${EXE_TARGET}
  cpp_cli/jdepp-app.cc
  ${JDEPP_SOURCES})
add_sanitizers(${EXE_TARGET})

target_include_directories(${EXE_TARGET} PRIVATE jdepp)

if (JDEPP_WITH_PYTHON)
  # pybind11 method:
  pybind11_add_module(${PY_TARGET} jdepp/python-binding-jdepp.cc ${JDEPP_SOURCES})

  # copy .so to jdepp/
  add_custom_command(
    TARGET ${PY_TARGET}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${PY_TARGET}>"
            "${CMAKE_SOURCE_DIR}/jdepp/$<TARGET_FILE_NAME:${PY_TARGET}>"
    COMMENT "copying jdepp python module file to jdepp/"
    VERBATIM)

endif()

# [VisualStudio]
if(WIN32)
  # Set ${EXE_TARGET} as a startup project for VS IDE
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${EXE_TARGET})

  # For easier debugging in VS IDE(cmake 3.8.0 or later required) Set working
  # directory where CMakeLists.txt is placed.
  if(CMAKE_VERSION VERSION_GREATER 3.8.0)
    set_target_properties(
      ${EXE_TARGET} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
                                           "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif()
