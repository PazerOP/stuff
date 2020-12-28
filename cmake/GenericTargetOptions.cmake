cmake_minimum_required(VERSION 3.17)

# target_include_directories
function(mh_target_include_directories targetName)

	get_target_property(TARGET_TYPE_VALUE ${targetName} TYPE)
	if (TARGET_TYPE_VALUE STREQUAL "INTERFACE_LIBRARY")
		target_include_directories(${targetName} INTERFACE ${ARGN})
	else()
		target_include_directories(${targetName} PUBLIC ${ARGN})
	endif()

endfunction()

# target_compile_features
function(mh_target_compile_features targetName)

	get_target_property(TARGET_TYPE_VALUE ${targetName} TYPE)
	if (TARGET_TYPE_VALUE STREQUAL "INTERFACE_LIBRARY")
		target_compile_features(${targetName} INTERFACE ${ARGN})
	else()
		target_compile_features(${targetName} PUBLIC ${ARGN})
	endif()

endfunction()

# target_compile_options
function(mh_target_compile_options targetName)

	get_target_property(TARGET_TYPE_VALUE ${targetName} TYPE)
	if (TARGET_TYPE_VALUE STREQUAL "INTERFACE_LIBRARY")
		target_compile_options(${targetName} INTERFACE ${ARGN})
	else()
		target_compile_options(${targetName} PUBLIC ${ARGN})
	endif()

endfunction()

# target_link_options
function(mh_target_link_options targetName)

	get_target_property(TARGET_TYPE_VALUE ${targetName} TYPE)
	if (TARGET_TYPE_VALUE STREQUAL "INTERFACE_LIBRARY")
		target_link_options(${targetName} INTERFACE ${ARGN})
	else()
		target_link_options(${targetName} PUBLIC ${ARGN})
	endif()

endfunction()

# target_compile_definitions
function(mh_target_compile_definitions targetName)

	get_target_property(TARGET_TYPE_VALUE ${targetName} TYPE)
	if (TARGET_TYPE_VALUE STREQUAL "INTERFACE_LIBRARY")
		target_compile_definitions(${targetName} INTERFACE ${ARGN})
	else()
		target_compile_definitions(${targetName} PUBLIC ${ARGN})
	endif()

endfunction()
