# - Try to find Allocore
# Once done this will define
#
#  ALLOCORE_FOUND - system has Allocore
#  ALLOCORE_INCLUDE_DIR - the Allocore include directory
#  ALLOCORE_LIBRARY - Link these to use Allocore
#

include(LibFindMacros)

# Dependencies
#libfind_package(LO Magick)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(ALLOCORE_PKGCONF liballocore)

# Include dir
find_path(ALLOCORE_INCLUDE_DIR
  NAMES
  allocore/al_Allocore.hpp
  PATHS
  ${CMAKE_SOURCE_DIR}/AlloSystem/allocore
  ${ALLOCORE_PKGCONF_INCLUDE_DIRS}
  ./
  ../allocore
  /usr/include
  /usr/local/include
  /opt/local/include
  ${ALLOCORE_ROOT}/build/include
  )

# Finally the library itself
find_library(ALLOCORE_LIBRARY
  NAMES
  allocore
  PATHS
  ${CMAKE_SOURCE_DIR}/AlloSystem/build/lib
  ${ALLOCORE_PKGCONF_LIBRARY_DIRS}
  ./build/lib
  ../build/lib
  /usr/lib
  /usr/local/lib
  /opt/local/lib
  ${ALLOCORE_ROOT}/build/lib
 )

# on OS X allocore needs the Cocoa framework
if(APPLE)
	find_library(COCOA_LIBRARY Cocoa)
	list(APPEND ALLOCORE_LIBRARY ${COCOA_LIBRARY})
endif()



#/usr/include/assimp
#/usr/local/include
#/opt/local/include/assimp
#/usr/local/Cellar/assimp/2.0.863/include/assimp

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(ALLOCORE_PROCESS_INCLUDES ALLOCORE_INCLUDE_DIR)
set(ALLOCORE_PROCESS_LIBS ALLOCORE_LIBRARY)
libfind_process(ALLOCORE)



