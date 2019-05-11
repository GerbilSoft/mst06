PROJECT(mst06)
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

# gcc byteswapping intrinsics.
IF(NOT MSVC)
	INCLUDE(CheckCSourceCompiles)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap16((unsigned short)argc); }" HAVE___BUILTIN_BSWAP16)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap32((unsigned int)argc); }" HAVE___BUILTIN_BSWAP32)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap64((unsigned long long)argc); }" HAVE___BUILTIN_BSWAP64)
ENDIF(NOT MSVC)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.byteswap.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.byteswap.h")

IF(NOT WIN32)
	# Non-Windows library checks.

	# On Win32, we use MultiByteToWideChar() and WideCharToMultiByte().
	# On other systems, we use iconv(), which could be in libc or libiconv.
	# Figure out which library has iconv().

	# First, check libc for iconv().
	INCLUDE(CheckLibraryExists)
	CHECK_LIBRARY_EXISTS(c iconv "" HAVE_ICONV_C)	# libc: iconv()
	IF(HAVE_ICONV_C)
		# iconv() found in libc.
		UNSET(ICONV_LIBRARY)
		SET(HAVE_ICONV 1)
	ELSE(HAVE_ICONV_C)
		# iconv() not found in libc.
		# Check in libiconv.
		CHECK_LIBRARY_EXISTS(iconv iconv "" HAVE_ICONV_LIBICONV)	# libiconv: iconv()
		IF(HAVE_ICONV_LIBICONV)
			# iconv() found in libiconv.
			SET(ICONV_LIBRARY "iconv")
			SET(HAVE_ICONV 1)
		ENDIF(HAVE_ICONV_LIBICONV)
	ENDIF(HAVE_ICONV_C)

	IF(NOT HAVE_ICONV)
		MESSAGE(FATAL_ERROR "iconv() not found, cannot continue.")
	ENDIF(NOT HAVE_ICONV)
ENDIF(NOT WIN32)

# Check for C library functions.
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(strnlen "string.h" HAVE_STRNLEN)

# Write the config.h file.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.mst06.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.mst06.h")

### Main executable. ###
SET(mst06_SRCS
	main.cpp
	Mst.cpp
	TextFuncs.cpp
	)
SET(mst06_H
	byteorder.h
	byteswap.h
	mst_structs.h
	Mst.hpp
	TextFuncs.hpp
	)

IF(WIN32)
	SET(mst06_OS_SRCS TextFuncs_win32.cpp)
ELSE(WIN32)
	SET(mst06_OS_SRCS TextFuncs_iconv.cpp)
ENDIF(WIN32)

ADD_EXECUTABLE(mst06
	${mst06_SRCS} ${mst06_H}
	${mst06_OS_SRCS}
	)
SET_PROPERTY(TARGET mst06 PROPERTY CXX_STANDARD 11)
TARGET_INCLUDE_DIRECTORIES(mst06 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

IF(ICONV_LIBRARY)
	TARGET_LINK_LIBRARIES(mst06 PRIVATE ${ICONV_LIBRARY})
ENDIF(ICONV_LIBRARY)

# TODO: Enable wildcard expansion on Windows?
INCLUDE(SetWindowsEntrypoint)
SET_WINDOWS_ENTRYPOINT(mst06 wmain OFF)