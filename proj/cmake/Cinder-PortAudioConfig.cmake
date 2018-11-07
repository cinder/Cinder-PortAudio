if( NOT TARGET Cinder-PortAudio )
	get_filename_component( PA_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/../../lib/portaudio" ABSOLUTE )
	get_filename_component( CI_PA_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE )
	get_filename_component( CINDER_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE )

	add_subdirectory( ${PA_LIB_PATH} ${CMAKE_CURRENT_BINARY_DIR}/portaudio EXCLUDE_FROM_ALL )
	add_library( Cinder-PortAudio 
					${CI_PA_SOURCE_PATH}/cinder/audio/ContextPortAudio.cpp 
					${CI_PA_SOURCE_PATH}/cinder/audio/DeviceManagerPortAudio.cpp 
	)
	
	target_include_directories( Cinder-PortAudio PUBLIC ${CI_PA_SOURCE_PATH} )
	target_include_directories( Cinder-PortAudio SYSTEM BEFORE PUBLIC ${CINDER_PATH}/include )

	if( NOT TARGET cinder )
		include( "${CINDER_PATH}/proj/cmake/configure.cmake" )
		find_package( cinder REQUIRED PATHS 
			"${CINDER_PATH}/${CINDER_LIB_DIRECTORY}"
			"$ENV{CINDER_PATH}/${CINDER_LIB_DIRECTORY}" 
		)
	endif()
	target_link_libraries( Cinder-PortAudio PRIVATE cinder portaudio )
endif()
