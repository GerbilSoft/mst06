PROJECT(extlib)

# Set common properties for extlib targets.
INCLUDE(SetMSVCDebugPath)
FUNCTION(SET_EXTLIB_PROPERTIES)
	FOREACH(_target ${ARGV})
		IF(TARGET ${_target})
			# Exclude from ALL builds.
			SET_TARGET_PROPERTIES(${_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
		ENDIF(TARGET ${_target})
	ENDFOREACH(_target ${ARGV})
ENDFUNCTION(SET_EXTLIB_PROPERTIES)

# Don't install any of these libraries by default.
# zlib and libpng will be installed if they're
# built as shared libraries.
SET(SKIP_INSTALL_LIBRARIES ON)
SET(SKIP_INSTALL_HEADERS ON)
SET(SKIP_INSTALL_FILES ON)
SET(SKIP_INSTALL_ALL ON)
SET(BUILD_SHARED_LIBS OFF)

# TODO: DirInstallPaths.
SET(CMAKE_INSTALL_BINDIR "bin")

# tinyxml2
IF(USE_INTERNAL_XML)
	# Use the internal copy of TinyXML2.
	# On Windows, this will build DLLs.
	# On other systems, this will be statically-linked.
	# TODO: Build a dylib for Mac OS X.
	IF(WIN32)
		SET(SKIP_INSTALL_LIBRARIES OFF)
		SET(SKIP_INSTALL_ALL OFF)
		SET(BUILD_STATIC_LIBS OFF)
		SET(BUILD_SHARED_LIBS ON)
	ELSE()
		SET(SKIP_INSTALL_LIBRARIES ON)
		SET(SKIP_INSTALL_ALL ON)
		SET(BUILD_STATIC_LIBS ON)
		SET(BUILD_SHARED_LIBS OFF)
	ENDIF()

	# Build TinyXML2.
	ADD_SUBDIRECTORY(tinyxml2)

	# NOTE: Cannot remove targets from all builds
	# if they're being installed.
	SET_EXTLIB_PROPERTIES(tinyxml2_static)
	#SET_EXTLIB_PROPERTIES(tinyxml2)

	# TODO: Set TinyXML2::tinyxml2 and use it in libromdata?
ENDIF(USE_INTERNAL_XML)
