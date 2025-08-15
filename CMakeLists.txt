PROJECT(mst06_base)
CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

LIST(APPEND CMAKE_MODULE_PATH
	"${CMAKE_SOURCE_DIR}/cmake/macros"
	"${CMAKE_SOURCE_DIR}/cmake/modules"
	"${CMAKE_SOURCE_DIR}/cmake/libs"
	)

# If no build type is set, default to "Debug".
# TODO: Default to "Release"?
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" TMP_BUILD_TYPE)
IF(TMP_BUILD_TYPE STREQUAL "")
	SET(CMAKE_BUILD_TYPE "Release")
ELSEIF(TMP_BUILD_TYPE MATCHES "none")
	SET(CMAKE_BUILD_TYPE "Release")
ENDIF()
UNSET(TMP_BUILD_TYPE)

# Put all the binaries and libraries into a single directory.
# NOTE: CACHE INTERNAL is required in order to get this to work
# for KDE5 for some reason. (and maybe that's why KDE4 did this
# layout by default?)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "Put all binaries in a single directory.")
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE INTERNAL "Put all libraries in a single directory.")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE INTERNAL "Put all archives in a single directory.")

# Program information.
SET(DESCRIPTION "Sonic '06 MST File Decoder and Encoder")
SET(PACKAGE_NAME "mst06")
SET(AUTHOR "David Korth")
SET(VERSION_MAJOR 0)
SET(VERSION_MINOR 0)
SET(VERSION_PATCH 0)
SET(VERSION_DEVEL 1)
IF(VERSION_PATCH)
	SET(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
ELSE(VERSION_PATCH)
	SET(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}")
ENDIF(VERSION_PATCH)
IF(VERSION_DEVEL)
	SET(VERSION_STRING "${VERSION_STRING}+")
ENDIF(VERSION_DEVEL)
SET(VERSION_STRING_WIN32 "${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH},${VERSION_DEVEL}")

# Split Debug macro.
# Also sets the image version for Windows builds.
INCLUDE(Win32ImageVersionLinkerFlags)
MACRO(DO_SPLIT_DEBUG _target)
	IF(TARGET ${_target})
		# Split debug information.
		INCLUDE(SetMSVCDebugPath)
		SET_MSVC_DEBUG_PATH(${_target})
		IF(SPLIT_DEBUG)
			INCLUDE(SplitDebugInformation)
			SPLIT_DEBUG_INFORMATION(${_target})
		ENDIF(SPLIT_DEBUG)
		# Set image version.
		# Subprojects can override ${VERSION_MAJOR} and ${VERSION_MINOR}.
		# FIXME: If minor version is e.g. "3", Windows interprets it as "03",
		# so "1.3" will actually be "1.03".
		WIN32_IMAGE_VERSION_LINKER_FLAGS(${VERSION_MAJOR} ${VERSION_MINOR})
	ENDIF(TARGET ${_target})
ENDMACRO(DO_SPLIT_DEBUG)

# Windows-specific functions.
INCLUDE(WindowsFunctions)

# Check for required libraries.
INCLUDE(CheckTinyXML2)

########### Add uninstall target ###############
CONFIGURE_FILE(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
	"${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake")

# Windows-specific compile flags.
IF(WIN32)
	SET(WIN32_C_FLAGS_COMMON "-DSTRICT -DWIN32_LEAN_AND_MEAN -DNOMINMAX")

	# NOTE: This program only supports Unicode on Windows.
	# No support for ANSI Windows, i.e. Win9x.
	SET(WIN32_C_FLAGS_COMMON "${WIN32_C_FLAGS_COMMON} -DUNICODE -D_UNICODE")

	# Minimum Windows version for the SDK is Windows 2000.
	SET(WIN32_C_FLAGS_COMMON "${WIN32_C_FLAGS_COMMON} -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 -D_WIN32_IE=0x0500")

	# Enable secure template overloads for C++.
	# References:
	# - MinGW's _mingw_secapi.h
	# - http://msdn.microsoft.com/en-us/library/ms175759%28v=VS.100%29.aspx
	SET(WIN32_CXX_FLAGS_COMMON "-D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES=1")
	SET(WIN32_CXX_FLAGS_COMMON "${WIN32_CXX_FLAGS_COMMON} -D_CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES_MEMORY=1")
	SET(WIN32_CXX_FLAGS_COMMON "${WIN32_CXX_FLAGS_COMMON} -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1")
	SET(WIN32_CXX_FLAGS_COMMON "${WIN32_CXX_FLAGS_COMMON} -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT=1")
	SET(WIN32_CXX_FLAGS_COMMON "${WIN32_CXX_FLAGS_COMMON} -D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_MEMORY=1")

	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WIN32_C_FLAGS_COMMON}")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WIN32_C_FLAGS_COMMON} ${WIN32_CXX_FLAGS_COMMON}")
ENDIF(WIN32)

# Disable unnecessary warnings.
INCLUDE(CheckCCompilerFlag)
IF(MSVC)
	SET(MSVC_C_FLAGS_COMMON "-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE")
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MSVC_C_FLAGS_COMMON}")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_C_FLAGS_COMMON}")

	# NOTE: /TSAWARE is automatically set for Windows 2000 and later. (as of at least Visual Studio .NET 2003)
	# NOTE 2: /TSAWARE is not applicable for DLLs.
	SET(MSVC_LINKER_FLAGS_COMMON "/NOLOGO /DYNAMICBASE /NXCOMPAT /LARGEADDRESSAWARE")
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MSVC_LINKER_FLAGS_COMMON}")
	SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_LINKER_FLAGS_COMMON}")
	SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${MSVC_LINKER_FLAGS_COMMON}")

	# Test for "/sdl" and "/guard:cf".
	CHECK_C_COMPILER_FLAG("/sdl" CFLAG_SDL)
	IF(CFLAG_SDL)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /sdl")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl")
	ENDIF(CFLAG_SDL)
	CHECK_C_COMPILER_FLAG("/guard:cf" CFLAG_GUARD_CF)
	IF(CFLAG_GUARD_CF)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /guard:cf")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /guard:cf")
		# "/guard:cf" must be added to linker flags as well.
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /guard:cf")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /guard:cf")
		SET(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /guard:cf")
	ENDIF(CFLAG_GUARD_CF)

	# Disable warning C4996 (deprecated), then re-enable it.
	# Otherwise, it gets handled as an error due to /sdl.
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996 /w34996")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996 /w34996")

	# MSVC 2015 uses thread-safe statics by default.
	# This doesn't work on XP, so disable it.
	IF(MSVC_VERSION GREATER 1899)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Zc:threadSafeInit-")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:threadSafeInit-")
	ENDIF()
ELSE(MSVC)
	CHECK_C_COMPILER_FLAG("-Wno-multichar" CFLAG_NO_MULTICHAR)
	IF(CFLAG_NO_MULTICHAR)
		SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar")
	ENDIF(CFLAG_NO_MULTICHAR)
ENDIF(MSVC)

# Project subdirectories.
ADD_SUBDIRECTORY(extlib)
ADD_SUBDIRECTORY(src)
