PROJECT(mst06)
CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

# gcc byteswapping intrinsics.
IF(NOT MSVC)
	INCLUDE(CheckCSourceCompiles)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap16((unsigned short)argc); }" HAVE___BUILTIN_BSWAP16)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap32((unsigned int)argc); }" HAVE___BUILTIN_BSWAP32)
	CHECK_C_SOURCE_COMPILES("int main(int argc, char *argv[]) { return __builtin_bswap64((unsigned long long)argc); }" HAVE___BUILTIN_BSWAP64)
ENDIF(NOT MSVC)

# Write the config.h files.
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.byteswap.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.byteswap.h")

### Main executable. ###
SET(mst06_SRCS mst06.cpp)
SET(mst06_H
	byteorder.h
	byteswap.h
	mst_structs.h
	)

ADD_EXECUTABLE(mst06 ${mst06_SRCS} ${mst06_H})
SET_PROPERTY(TARGET mst06 PROPERTY CXX_STANDARD 11)
TARGET_INCLUDE_DIRECTORIES(mst06 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
