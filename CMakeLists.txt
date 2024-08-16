# CMakeList.txt : CMake project for search_server, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (search_server)
aux_source_directory(search-server SOURCE_FILES)
# Add source to this project's executable.
add_executable (search_server ${SOURCE_FILES})
# Set include directories
target_include_directories(search_server PRIVATE
    ${CMAKE_SOURCE_DIR}/search-server
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET search_server PROPERTY CXX_STANDARD 17)
endif()

# TODO: Add tests and install targets if needed.
