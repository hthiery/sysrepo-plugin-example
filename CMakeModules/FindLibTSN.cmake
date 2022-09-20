#  LIBTSN_FOUND - System has LIBTSN
#  LIBTSN_INCLUDE_DIRS - The LIBTSN include directories
#  LIBTSN_LIBRARIES - The libraries needed to use LIBTSN
#  LIBTSN_DEFINITIONS - Compiler switches required for using LIBTSN

find_package(PkgConfig)
pkg_check_modules(PC_LIBTSN QUIET libtsn)
set(LIBTSN_DEFINITIONS ${PC_LIBTSN_CFLAGS_OTHER})

find_path(LIBTSN_INCLUDE_DIR tsn/genl_tsn.h
	HINTS ${PC_LIBTSN_INCLUDEDIR} ${PC_LIBTSN_INCLUDE_DIRS}
    PATH_SUFFIXES tsn )

find_library(LIBTSN_LIBRARY NAMES libtsn.so
    HINTS ${PC_LIBTSN_LIBDIR} ${PC_LIBTSN_LIBRARY_DIRS} )

set(LIBTSN_LIBRARIES ${LIBTSN_LIBRARY} )
set(LIBTSN_INCLUDE_DIRS ${LIBTSN_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBTSN_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibTSN DEFAULT_MSG
    LIBTSN_LIBRARY LIBTSN_INCLUDE_DIR)

mark_as_advanced(LIBTSN_INCLUDE_DIR LIBTSN_LIBRARY)
