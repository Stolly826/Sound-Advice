# CMakeList.txt : CMake project for SoundAdviceMock, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("SoundAdviceMock")

# TagLib (audio metadata). Downloaded and built as a static library at configure time,
# so nobody has to install it. Pinned to a release tag so everyone builds the same version.
include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE) # only needed for compressed ID3v2 frames

FetchContent_Declare(
  taglib
  GIT_REPOSITORY https://github.com/taglib/taglib.git
  GIT_TAG        v2.3.2
  GIT_SHALLOW    TRUE
  GIT_SUBMODULES "3rdparty/utfcpp"
)
FetchContent_MakeAvailable(taglib)

# Add source to this project's executable.
add_executable (SoundAdviceMock "SoundAdviceMock.cpp" "SoundAdviceMock.h" "AudioMetadata.cpp" "AudioMetadata.h")

# When built in-tree, TagLib's `tag` target doesn't export its header dirs, so borrow them.
get_target_property(TAGLIB_INCLUDE_DIRS tag INCLUDE_DIRECTORIES)
target_include_directories(SoundAdviceMock SYSTEM PRIVATE "${taglib_SOURCE_DIR}/taglib" ${TAGLIB_INCLUDE_DIRS})
target_link_libraries(SoundAdviceMock PRIVATE tag)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET SoundAdviceMock PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
