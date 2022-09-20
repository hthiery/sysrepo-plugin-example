#  SYSREPO_FOUND - System has SYSREPO
#  SYSREPO_INCLUDE_DIRS - The SYSREPO include directories
#  SYSREPO_LIBRARIES - The libraries needed to use SYSREPO
#  SYSREPO_DEFINITIONS - Compiler switches required for using SYSREPO

find_package(PkgConfig)
pkg_check_modules(PC_CJSON QUIET libcjson)
set(CJSON_DEFINITIONS ${PC_CJSON_CFLAGS_OTHER})

find_path(CJSON_INCLUDE_DIR cJSON.h
          HINTS ${PC_CJSON_INCLUDEDIR} ${PC_CJSON_INCLUDE_DIRS}
          PATH_SUFFIXES cjson)

find_library(CJSON_LIBRARY NAMES cjson
          HINTS ${PC_CJSON_LIBDIR} ${PC_CJSON_LIBRARY_DIRS} )

set(CJSON_LIBRARIES ${CJSON_LIBRARY} )
set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(cjson  DEFAULT_MSG
	CJSON_LIBRARY CJSON_INCLUDE_DIR)

mark_as_advanced(CJSON_INCLUDE_DIR CJSON_LIBRARY)
