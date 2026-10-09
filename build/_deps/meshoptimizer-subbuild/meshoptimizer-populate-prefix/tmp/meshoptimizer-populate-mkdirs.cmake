# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-src")
  file(MAKE_DIRECTORY "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-src")
endif()
file(MAKE_DIRECTORY
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-build"
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix"
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/tmp"
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/src/meshoptimizer-populate-stamp"
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/src"
  "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/src/meshoptimizer-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/src/meshoptimizer-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/smarttapon/coding_prac/terPiece/build/_deps/meshoptimizer-subbuild/meshoptimizer-populate-prefix/src/meshoptimizer-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
