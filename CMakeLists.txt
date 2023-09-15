include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckLibraryExists)

cmake_minimum_required(VERSION 3.12)

set(V8_BASEDIR "" CACHE PATH "Path to previously compiled v8")

if (V8_BASEDIR STREQUAL "")
	message(FATAL_ERROR "V8_BASEDIR argument is mandatory")
endif()

#set(CMAKE_CPP_COMPILER  ${V8_BASEDIR}/third_party/llvm-build/Release+Asserts/bin/clang++ )
#set(CMAKE_CXX_COMPILER  ${V8_BASEDIR}/third_party/llvm-build/Release+Asserts/bin/clang++ )
#set(CMAKE_C_COMPILER	${V8_BASEDIR}/third_party/llvm-build/Release+Asserts/bin/clang )

set(PostgreSQL_ADDITIONAL_VERSIONS "13" "14")
set(PostgreSQL_ROOT ${PostgreSQL_ROOT} /usr/local/opt/libpq) # TODO HACK MAC OS
set(PostgreSQL_LIBRARY ${PostgreSQL_LIBRARY} /usr/local/opt/libpq/lib) # TODO HACK MAC OS
set(PostgreSQL_INCLUDE_DIR ${PostgreSQL_INCLUDE_DIR} /usr/local/opt/libpq/include) # TODO HACK MAC OS
project(tea)

find_path(PostgreSQL_TYPE_INCLUDE_DIR
	NAMES catalog/pg_type.h
	PATHS
		# Look in other places.
		${PostgreSQL_ROOT_DIRECTORIES}
	PATH_SUFFIXES
		postgresql
		pgsql/server
		postgresql/server
		postgresql/13/server
		include/server
	# Help the user find it if we cannot.
	DOC "The ${PostgreSQL_INCLUDE_DIR_MESSAGE}"
)
find_path(PostgreSQL_PQ_FE_INCLUDE_DIR
	NAMES libpq-fe.h
	PATHS
		# Look in other places.
		${PostgreSQL_ROOT_DIRECTORIES}
		/usr/local/Cellar/libpq/14.3 # TODO HACK MAC OS
	PATH_SUFFIXES
		postgresql
		pgsql/server
		postgresql/server
		postgresql/13/server
		include/server
		include
	# Help the user find it if we cannot.
	DOC "The ${PostgreSQL_INCLUDE_DIR_MESSAGE}"
)
find_package(PostgreSQL REQUIRED)

find_path(V8_INCLUDE_DIR
	NAMES v8.h
	PATHS
		${V8_BASEDIR}
	PATH_SUFFIXES
		include
	DOC "Path to V8 includes"
	REQUIRED
)

find_path(V8_SHARED_LIB_DIR
	NAMES
		libv8.so
	PATHS
		${V8_BASEDIR}
	PATH_SUFFIXES
		out/x64.release
		out/x64.release/obj
		out/x64.release.sample
		out/x64.release.sample/obj
		out/x64.debug
		out/x64.debug/obj
		out.gn/x64.release/obj
		out.gn/x64.release.sample/obj
		out.gn/x64.debug/obj
	DOC "Path to compiled shared V8 libraries"
)


find_path(V8_MONOLITH_LIB_DIR
	NAMES
		libv8_monolith.a
	PATHS
		${V8_BASEDIR}
	PATH_SUFFIXES
		out.gn/x64.release.sample/obj/		
		out/x64.release
		out/x64.release/obj
		out/x64.release.sample
		out/x64.release.sample/obj
		out/x64.debug
		out/x64.debug/obj
		out.gn/x64.release/obj
		out.gn/x64.release.sample/obj
		out.gn/x64.debug/obj
	DOC "Path to compiled static V8 libraries"
)


if (NOT V8_MONOLITH_LIB_DIR STREQUAL "V8_MONOLITH_LIB_DIR-NOTFOUND")
	set(V8_LIB_DIR ${V8_MONOLITH_LIB_DIR})
	add_library(libv8_monolith	SHARED IMPORTED GLOBAL)
	set(V8_LIBRARIES "v8_monolith")
elseif (NOT V8_SHARED_LIB_DIR STREQUAL "V8_SHARED_LIB_DIR-NOTFOUND")
	set(V8_LIB_DIR ${V8_SHARED_LIB_DIR})
	add_library(libv8 SHARED IMPORTED GLOBAL)
	add_library(v8_libplatform SHARED IMPORTED GLOBAL)
	set(V8_LIBRARIES "v8 -lv8_libplatform") #TODO there should be some other way rather then -l
else()
	message(FATAL_ERROR "V8 static or shared libraries not found")
endif()

#message(WARNING ${V8_LIB_DIR})
file(REMOVE ../lib/*.${CMAKE_SHARED_LIBRARY_SUFFIX})

check_symbol_exists(sleep unistd.h HAVE_SLEEP)
check_symbol_exists(inet_pton arpa/inet.h HAVE_PTON)
check_symbol_exists(inet_ntop arpa/inet.h HAVE_NTOP)
check_include_file(sys/mman.h HAVE_MMAN)

check_include_file(fcgi_stdio.h HAVE_FASTCGI)


# For MAC OS (brew install libpq)
#export LDFLAGS="-L/usr/local/opt/libpq/lib"
#export CPPFLAGS="-I/usr/local/opt/libpq/include"



include_directories(AFTER SYSTEM ${V8_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/ ${PostgreSQL_TYPE_INCLUDE_DIR} ${PostgreSQL_PQ_FE_INCLUDE_DIR})
link_directories( ${V8_LIB_DIR} )

#FLAGS= -DCONFIG_PATH=/etc/teajs.conf -DDSO_EXT=so -DHAVE_SLEEP -DHAVE_PTON -DHAVE_NTOP -DHAVE_MMAN_H -DFASTCGI_JS -pthread -std=c++14 -DV8_COMPRESS_POINTERS -I/download/v8-build/v8/include -I/home/projects/teajs/src -fPIC -ggdb -Wno-unused-result -I/usr/include/postgresql/13/server/ -I/usr/include/postgresql/
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++17 -fPIC -ggdb -Wno-unused-result -pthread")

add_library(libpthread		SHARED IMPORTED GLOBAL)
add_library(libdl			SHARED IMPORTED GLOBAL)
add_library(libfcgi			SHARED IMPORTED GLOBAL)


file(MAKE_DIRECTORY lib)
# TODO copy *.js files


add_executable(tea src/common.cc src/system.cc src/cache.cc src/gc.cc src/app.cc src/path.cc src/lib/binary/bytestorage.cc src/teajs.cc)
add_library(libtea SHARED src/common.cc src/system.cc src/cache.cc src/gc.cc src/app.cc src/path.cc src/lib/binary/bytestorage.cc)

target_link_libraries(tea PUBLIC libtea pthread dl fcgi ${V8_LIBRARIES})

string(REGEX REPLACE "\\." "" CMAKE_SHARED_LIBRARY_SUFFIX_ONLY ${CMAKE_SHARED_LIBRARY_SUFFIX})

add_compile_definitions(CONFIG_PATH=/home/vahvarh/try_teajs/teajs/teajs.conf FASTCGI_JS V8_COMPRESS_POINTERS DSO_EXT=${CMAKE_SHARED_LIBRARY_SUFFIX_ONLY})
#add_compile_definitions(CONFIG_PATH=/etc/teajs.conf FASTCGI_JS V8_COMPRESS_POINTERS)
if(HAVE_SLEEP)
	add_compile_definitions(HAVE_SLEEP)
endif()
if(HAVE_PTON)
	add_compile_definitions(HAVE_PTON)
endif()
if(HAVE_NTOP)
	add_compile_definitions(HAVE_NTOP)
endif()
if(HAVE_MMAN)
	add_compile_definitions(HAVE_MMAN)
endif()
if(HAVE_FASTCGI)
	message(WARNING "have fastcgi")
	add_compile_definitions(FASTCGI_JS)
endif()

add_library(libbinary	SHARED src/lib/binary/binary.cc src/lib/binary/bytestorage.cc)
add_library(libfs		SHARED src/lib/fs/fs.cc src/path.cc src/lib/binary/bytestorage.cc)
add_library(libgd		SHARED src/lib/gd/gd.cc)
add_library(libprocess	SHARED src/lib/process/process.cc)
add_library(libsocket	SHARED src/lib/socket/socket.cc)
add_library(libpgsql	SHARED src/lib/pgsql/pgsql.cc)
add_library(libtls		SHARED src/lib/tls/tls.cc)
add_library(libzlib		SHARED src/lib/zlib/zlib.cc)
add_library(libcurses		SHARED src/lib/curses/curses.cc)

#target_compile_definitions(tea PUBLIC FLAGS= -DCONFIG_PATH=/etc/teajs.conf -DDSO_EXT=${CMAKE_SHARED_LIBRARY_SUFFIX} -DFASTCGI_JS -pthread -std=c++14 -DV8_COMPRESS_POINTERS -fPIC -ggdb -Wno-unused-result)
#target_compile_definitions(tea PUBLIC ${HAVE_SLEEP} ${HAVE_PTON} ${HAVE_NTOP} ${HAVE_MMAN})
#-DHAVE_SLEEP -DHAVE_PTON -DHAVE_NTOP -DHAVE_MMAN_H

#file(GLOB files "liblib*.${CMAKE_SHARED_LIBRARY_SUFFIX}")
#message(WARNING "liblib files are: " ${files})
#foreach(file ${files})
#	string(REGEX REPLACE "liblib" "" dstfile ${file})
#	install(TARGETS ${file} DESTINATION ../lib/ RENAME ${dstfile})
#endforeach()

#file(COPY libbinary DESTINATION ../lib/ RENAME binary.${CMAKE_SHARED_LIBRARY_SUFFIX})
#file(RENAME liblibbinary${CMAKE_SHARED_LIBRARY_SUFFIX} ../lib/binary${CMAKE_SHARED_LIBRARY_SUFFIX})

#add_custom_command(
#	TARGET ${PROJECT_NAME}
#	POST_BUILD
#	COMMAND
#		../libs-copy.pl ${CMAKE_CURRENT_BINARY_DIR}/liblib*.${CMAKE_SHARED_LIBRARY_SUFFIX}
#)
