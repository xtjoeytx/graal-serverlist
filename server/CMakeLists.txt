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
)

set(
    HEADERS
		${PROJECT_BINARY_DIR}/server/include/IConfig.h
		include/ListServer.h
		include/IDataBackend.h
		include/PlayerProfile.h
		include/ServerPlayer.h
		include/IrcChannel.h
		include/PlayerConnection.h
		include/ServerConnection.h
		include/IrcConnection.h
        include/IrcStub.h)

include_directories(
    # Include the CMake-generated version header from the build directory
    ${PROJECT_BINARY_DIR}/server/include
	include
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

check_symbol_exists(inet_pton "ws2tcpip.h" HAVE_INET_PTON)
if (NOT HAVE_INET_PTON)
	check_function_exists(inet_pton HAVE_INET_PTON)
endif()

if (HAVE_INET_PTON)
	add_definition(-DHAVE_INET_PTON)
endif()


if(NOT NOMYSQL)
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
	endif()
endif()

include_directories(${PROJECT_SOURCE_DIR}/dependencies/gs2lib/include)
include_directories(${PROJECT_SOURCE_DIR}/dependencies/mysql-modern-cpp/mysql+++)

if(APPLE)
	add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

    # Enable ARC (automatic reference counting) for OS X build
    set_property(
        TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc"
    )
elseif(WIN32)
    add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})
	if(MSVC)
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
		set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "../bin")
	endif()
elseif(EMSCRIPTEN)
	add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})
else()
	add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})
endif()

add_dependencies(${TARGET_NAME} gs2lib)
target_link_libraries(${TARGET_NAME} gs2lib)

if(NOT NOMYSQL)
	if(MYSQL_FOUND)
		target_link_libraries(${TARGET_NAME} ${MYSQL_LIBRARY})
	else()
		add_dependencies(${TARGET_NAME} mariadbclient)
		target_link_libraries(${TARGET_NAME} mariadbclient)
	endif()
endif()

target_link_libraries(${TARGET_NAME} ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
	target_link_libraries(${TARGET_NAME} ws2_32 wsock32 iphlpapi)
endif()

file(GLOB TEXT
    "${PROJECT_NAME_LOWER}.wasm"
)

set(INSTALL_DEST ".")

install(FILES ${TEXT} DESTINATION ${INSTALL_DEST})

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_DEST})
