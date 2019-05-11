# Set Windows entrypoint.
# _target: Target.
# _entrypoint: Entry point. (main, wmain, WinMain, wWinMain)
# _setargv: If true, link to setargv.obj/wsetargv.obj in MSVC builds.
FUNCTION(SET_WINDOWS_ENTRYPOINT _target _entrypoint _setargv)
IF(WIN32)
	IF(MSVC)
		# MSVC automatically prepends an underscore if necessary.
		SET(ENTRY_POINT_FLAG "/ENTRY:${_entrypoint}CRTStartup")
		IF(_setargv)
			IF(_entrypoint MATCHES ^wmain OR _entrypoint MATCHES ^wWinMain)
				SET(SETARGV_FLAG "wsetargv.obj")
			ELSE()
				SET(SETARGV_FLAG "setargv.obj")
			ENDIF()
		ENDIF(_setargv)
		UNSET(UNICODE_FLAG)
	ELSE(MSVC)
		# MinGW does not automatically prepend an underscore.
		# TODO: Does ARM Windows have a leading underscore?
		# TODO: _setargv for MinGW.

		# NOTE: MinGW uses separate crt*.o files for Unicode
		# instead of a separate entry point.
		IF(_entrypoint MATCHES "^w")
			SET(UNICODE_FLAG "-municode")
			STRING(SUBSTRING "${_entrypoint}" 1 -1 _entrypoint)
		ENDIF()

		IF(CPU_i386 OR CPU_amd64)
			IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
				SET(ENTRY_POINT "_${_entrypoint}CRTStartup")
			ELSE()
				SET(ENTRY_POINT "${_entrypoint}CRTStartup")
			ENDIF()
		ELSE()
			SET(ENTRY_POINT "${_entrypoint}CRTStartup")
		ENDIF(CPU_i386 OR CPU_amd64)
		SET(ENTRY_POINT_FLAG "-Wl,-e,${ENTRY_POINT}")
		UNSET(SETARGV_FLAG)
	ENDIF(MSVC)

	GET_TARGET_PROPERTY(TARGET_LINK_FLAGS ${_target} LINK_FLAGS)
	IF(TARGET_LINK_FLAGS)
		SET(TARGET_LINK_FLAGS "${TARGET_LINK_FLAGS} ${UNICODE_FLAG} ${ENTRY_POINT_FLAG} ${SETARGV_FLAG}")
	ELSE()
		SET(TARGET_LINK_FLAGS "${UNICODE_FLAG} ${ENTRY_POINT_FLAG} ${SETARGV_FLAG}")
	ENDIF()
	SET_TARGET_PROPERTIES(${_target} PROPERTIES LINK_FLAGS "${TARGET_LINK_FLAGS}")
ENDIF(WIN32)
ENDFUNCTION()
