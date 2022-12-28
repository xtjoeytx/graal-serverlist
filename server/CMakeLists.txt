#
#  server/src/CMakeLists.txt
#
#  Copyright 2019 死体
#
#  This file is part of GS2Emu.
#
#  GS2Emu is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  GS2Emu is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with GS2Emu.  If not, see <http://www.gnu.org/licenses/>.
#

include(CheckFunctionExists)
include(CheckSymbolExists)

set(
	SOURCES
	src/main.cpp
	src/ListServer.cpp
	src/ServerPlayer.cpp
	src/IrcChannel.cpp
	src/PlayerConnection.cpp
	src/ServerConnection.cpp
	src/IrcConnection.cpp
	src/IrcServer.cpp
	src/RealIrcStub.cpp
	src/SimulatedIrcStub.cpp
	src/ClientType.cpp
)


set(EXE_HEADERS "")
set(
	HEADERS
	${PROJECT_BINARY_DIR}/server/include/IConfig.h
	include/main.h
	include/ListServer.h
	include/IDataBackend.h
	include/PlayerProfile.h
	include/ServerPlayer.h
	include/IrcChannel.h
	include/PlayerConnection.h
	include/ServerConnection.h
	include/IrcServer.h
	include/IrcConnection.h
	include/IrcStub.h
	include/RealIrcStub.h
	include/SimulatedIrcStub.h
	include/ServerHQ.h
	include/ClientType.h)

check_symbol_exists(inet_pton "ws2tcpip.h" HAVE_INET_PTON)
if (NOT HAVE_INET_PTON)
	check_function_exists(inet_pton HAVE_INET_PTON)
endif()

if (HAVE_INET_PTON)
	add_definition(-DHAVE_INET_PTON)
endif()


if(MYSQL)
	list(
		APPEND
		SOURCES
		src/MySQLBackend.cpp
	)

	list(
		APPEND
		HEADERS
		include/MySQLBackend.h
	)

	if(NOT MYSQL_FOUND)
		configure_file(
			${PROJECT_SOURCE_DIR}/dependencies/mysqlclient/include/mysql.h
			${PROJECT_BINARY_DIR}/dependencies/mysqlclient/include/mysql/mysql.h
		)
		configure_file(
			${PROJECT_SOURCE_DIR}/dependencies/mysqlclient/include/errmsg.h
			${PROJECT_BINARY_DIR}/dependencies/mysqlclient/include/mysql/errmsg.h
		)
		include_directories(${PROJECT_SOURCE_DIR}/dependencies/mysqlclient/include)
		include_directories(${PROJECT_BINARY_DIR}/dependencies/mysqlclient/include)
	else()
		include_directories(${MYSQL_INCLUDE_DIR})
	endif()
endif()

include_directories(
	# Include the CMake-generated version header from the build directory
	${PROJECT_BINARY_DIR}/server/include
	${PROJECT_SOURCE_DIR}/server/include
)

# Set target names for the executables
if(APPLE OR WIN32)
	# OS X and Windows get a mixed-case binary name
	set(TARGET_NAME ${PROJECT_NAME})
elseif(EMSCRIPTEN)
	set(TARGET_NAME ${PROJECT_NAME_LOWER})
else()
	# Linux/other UNIX get a lower-case binary name
	set(TARGET_NAME ${PROJECT_NAME_LOWER})
endif()

set(TARGET_NAME_OLD ${TARGET_NAME})
set(TARGET_NAME ${TARGET_NAME}_lib)



include_directories(${PROJECT_SOURCE_DIR}/dependencies/gs2lib/include)
include_directories(${PROJECT_SOURCE_DIR}/dependencies/mysql-modern-cpp/mysql+++)

if(APPLE)
	add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})

	# Enable ARC (automatic reference counting) for OS X build
	set_property(
		TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc"
	)
elseif(WIN32)
	if(MINGW)
		# Generate version header from the above
		configure_file(
				${PROJECT_SOURCE_DIR}/server/include/windresrc.h.in
				${PROJECT_BINARY_DIR}/windresrc.h
		)
		configure_file(
				${PROJECT_SOURCE_DIR}/my.rc.in
				${PROJECT_BINARY_DIR}/main.rc
		)
		file(COPY
				${PROJECT_SOURCE_DIR}/listserver.ico
				DESTINATION
				${PROJECT_BINARY_DIR}/
				)
		list(APPEND EXE_HEADERS
				${PROJECT_BINARY_DIR}/windresrc.h
				${PROJECT_BINARY_DIR}/main.rc
				)
		set(CMAKE_RC_COMPILER_INIT windres)

		ENABLE_LANGUAGE(RC)
		SET(CMAKE_RC_COMPILE_OBJECT
				"<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
	endif()

	add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})

	if(MSVC)
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
	endif()
elseif(EMSCRIPTEN)
	add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})
else()
	add_library(${TARGET_NAME} STATIC ${SOURCES} ${HEADERS})
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE NOMAIN)

add_executable(${TARGET_NAME_OLD} src/main.cpp ${EXE_HEADERS})
target_link_libraries(${TARGET_NAME_OLD} PUBLIC ${TARGET_NAME})

target_link_libraries(${TARGET_NAME} ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
	target_link_libraries(${TARGET_NAME} ws2_32 wsock32 iphlpapi)
endif()

target_include_directories(${TARGET_NAME} PUBLIC ${GS2LIB_INCLUDE_DIRECTORY})

add_dependencies(${TARGET_NAME} gs2lib)
target_link_libraries(${TARGET_NAME} gs2lib)

if(MYSQL)
	if(MYSQL_FOUND)
		target_link_libraries(${TARGET_NAME} ${MYSQL_LIBRARY} ssl crypto zstd)
	else()
		add_dependencies(${TARGET_NAME} mariadbclient)
		target_link_libraries(${TARGET_NAME} mariadbclient)
	endif()
endif()

if (MINGW)
	target_link_options(${TARGET_NAME} PRIVATE -static -fstack-protector)
	target_link_libraries(${TARGET_NAME} -static-libgcc -static-libstdc++)
	target_link_options(${TARGET_NAME_OLD} PRIVATE -static -fstack-protector)
	target_link_libraries(${TARGET_NAME_OLD} PUBLIC -static-libgcc -static-libstdc++)

	install(CODE "set(MY_EXE \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET_NAME_OLD}.exe\")")

	# Transfer the value of ${MY_DEPENDENCY_PATHS} into the install script
	install(CODE "set(MY_DEPENDENCY_PATHS \"${CMAKE_FIND_DLL_PATH}\")")
	install(CODE [[
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
        set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "./objdump_unix2dos.sh")

        write_file("./objdump_unix2dos.sh" "${CMAKE_OBJDUMP} $@ | unix2dos")
        file(CHMOD "./objdump_unix2dos.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

	  function(install_library_with_deps LIBRARY)
	  	  message("Getting dependencies for ${LIBRARY}")

		file(INSTALL
		  DESTINATION "${CMAKE_INSTALL_PREFIX}/"
		  TYPE SHARED_LIBRARY
		  FOLLOW_SYMLINK_CHAIN
		  FILES "${LIBRARY}"
		)
		file(GET_RUNTIME_DEPENDENCIES
		  LIBRARIES ${LIBRARY}
		  RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
		  UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
		  DIRECTORIES ${MY_DEPENDENCY_PATHS}
		)
		foreach(FILE ${RESOLVED_DEPS})
		  if(NOT IS_SYMLINK ${FILE})
			install_library_with_deps(${FILE})
		  else()
		  	message( "Symlink ${LIBRARY}: ${FILE}")
		  endif()
		endforeach()
		foreach(FILE ${UNRESOLVED_DEPS})
		  message( "Unresolved from ${LIBRARY}: ${FILE}")
		endforeach()
	  endfunction()
	  message("Getting dependencies for ${MY_EXE}")
	  file(GET_RUNTIME_DEPENDENCIES
			EXECUTABLES ${MY_EXE}
		RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
		UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
		    DIRECTORIES ${MY_DEPENDENCY_PATHS}

	  )
	  foreach(FILE ${RESOLVED_DEPS})
		install_library_with_deps(${FILE})
	  endforeach()
	  foreach(FILE ${UNRESOLVED_DEPS})
		message( "Unresolved: ${FILE}")
	  endforeach()
	]])
endif()


set(APP_LIBRARY_NAME
		"${TARGET_NAME}"
		PARENT_SCOPE)

file(GLOB TEXT
	"${PROJECT_NAME_LOWER}.wasm"
)

set(INSTALL_DEST ".")

install(FILES ${TEXT} DESTINATION ${INSTALL_DEST})

set(INSTALL_DEST .)

install(TARGETS ${TARGET_NAME_OLD} DESTINATION ${INSTALL_DEST})

