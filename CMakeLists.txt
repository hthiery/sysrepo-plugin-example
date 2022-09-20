CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

project(sysrepo-plugin-interfaces C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")

include(GNUInstallDirs)
include(CheckFunctionExists)
include(CheckIncludeFile)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

set(YANG_MODULE_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/yang/modules/netopeer2" CACHE STRING "Directory where to copy the YANG modules to")

set(SCRIPT_DIR "${CMAKE_INSTALL_PREFIX}/share/netopeer2" CACHE STRING "Directory where to copy the install scripts to")

set(PLUGIN 0 CACHE BOOL "Build a plugin")

find_package(PkgConfig REQUIRED)

execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
	"--variable=SRPD_PLUGINS_PATH" "sysrepo"
	OUTPUT_VARIABLE SRPD_PLUGINS_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE)
message("SRPD_PLUGINS_PATH: ${SRPD_PLUGINS_PATH}")

execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE}
	"--variable=SR_PLUGINS_PATH" "sysrepo"
	OUTPUT_VARIABLE SR_PLUGINS_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE)
message("SR_PLUGINS_PATH: ${SR_PLUGINS_PATH}")

if(PLUGIN)
	add_library(${CMAKE_PROJECT_NAME} MODULE)
	target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PLUGIN=1)
	install(TARGETS ${CMAKE_PROJECT_NAME} LIBRARY
		DESTINATION ${SRPD_PLUGINS_PATH})
else()
	add_executable(${CMAKE_PROJECT_NAME})
	install(TARGETS ${CMAKE_PROJECT_NAME}
		DESTINATION usr/bin)
endif()

target_sources(${CMAKE_PROJECT_NAME}
	PRIVATE
		src/ietf-interfaces-plugin.c
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME} PREFIX "")


find_package(Sysrepo REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} ${SYSREPO_LIBRARIES})
include_directories(${SYSREPO_INCLUDE_DIRS})

find_package(LibYANG REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBYANG_LIBRARIES})
include_directories(${LIBYANG_INCLUDE_DIRS})


include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")

configure_file("${PROJECT_SOURCE_DIR}/src/config.h.in" "${PROJECT_BINARY_DIR}/include/config.h" ESCAPE_QUOTES @ONLY)

# script options
option(INSTALL_MODULES "Enable yang model installation" ON)
set(MODULES_PERMS 600 CACHE STRING "File access permissions set for all the server modules")
if(NOT MODULES_OWNER)
	execute_process(COMMAND id -un RESULT_VARIABLE RET
		OUTPUT_VARIABLE MODULES_OWNER OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_VARIABLE ERROR_STR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(RET)
		message(WARNING "Learning server module user failed (${ERROR_STR}), the current user will be used.")
	endif()
endif()
set(MODULES_OWNER "${MODULES_OWNER}" CACHE STRING "System user that will become the owner of server modules, empty means the current user")
if(NOT MODULES_GROUP AND MODULES_OWNER)
	execute_process(COMMAND id -gn ${MODULES_OWNER} RESULT_VARIABLE RET
		OUTPUT_VARIABLE MODULES_GROUP OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_VARIABLE ERROR_STR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(RET)
		message(WARNING "Learning server module group failed (${ERROR_STR}), the current user group will be used.")
	endif()
endif()
set(MODULES_GROUP "${MODULES_GROUP}" CACHE STRING "System group that the server modules will belong to, empty means the current user group")


# install the module files
install(DIRECTORY "${PROJECT_SOURCE_DIR}/modules/" DESTINATION ${YANG_MODULE_DIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/scripts/" DESTINATION ${SCRIPT_DIR} USE_SOURCE_PERMISSIONS)

if (INSTALL_MODULES)
    install(CODE "
		message(STATUS \"Installing missing sysrepo modules...\")
		set(ENV{NP2_MODULE_DIR} ${YANG_MODULE_DIR})
		set(ENV{NP2_MODULE_PERMS} ${MODULES_PERMS})
		set(ENV{NP2_MODULE_OWNER} ${MODULES_OWNER})
		set(ENV{NP2_MODULE_GROUP} ${MODULES_GROUP})
		execute_process(COMMAND \"\$ENV{DESTDIR}${SCRIPT_DIR}/model-install.sh\" RESULT_VARIABLE SETUP_RES)
		if(NOT SETUP_RES EQUAL \"0\")
			message(FATAL_ERROR \" scripts/model-install.sh failed: \${SETUP_RES}\")
		endif()
	")
endif()
