cmake_minimum_required(VERSION 3.17)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

function(mh_basic_install)
	cmake_parse_arguments(PARSE_ARGV 0
		# arg variable prefix
		"arg"
		# options
		""
		# one value keywords
		"PROJ_NAME;PROJ_VERSION"
		# multi-value keywords
		"PROJ_INCLUDE_DIRS;FIND_DEPENDENCIES"
	)

	if (DEFINED arg_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION} was passed extra arguments (${arg_UNPARSED_ARGUMENTS})")
	endif()

	if (NOT DEFINED arg_PROJ_NAME)
		if (NOT DEFINED PROJECT_NAME)
			message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION} was not passed PROJ_NAME argument, and PROJECT_NAME was not defined")
		endif()

		set(arg_PROJ_NAME "${PROJECT_NAME}")
	endif()

	if (NOT DEFINED arg_PROJ_VERSION)
		if (NOT DEFINED PROJECT_VERSION)
			message(FATAL_ERROR "${CMAKE_CURRENT_FUNCTION} was not passed PROJ_VERSION argument, and PROJECT_VERSION was not defined")
		endif()

		set(arg_PROJ_VERSION "${PROJECT_VERSION}")
	endif()

	SET(NAMESPACE "mh")
	set(FULL_PROJ_NAME "${arg_PROJ_NAME}")
	add_library("${NAMESPACE}::${FULL_PROJ_NAME}" ALIAS "${FULL_PROJ_NAME}")

	# Dependencies the installed package config must find_dependency() before
	# importing the exported targets (e.g. link interface dependencies).
	set(MH_BASIC_INSTALL_FIND_DEPENDENCIES "${arg_FIND_DEPENDENCIES}")

	configure_package_config_file(
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mh-BasicInstall-config.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/${FULL_PROJ_NAME}-config.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/${FULL_PROJ_NAME}"
	)

	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/${FULL_PROJ_NAME}-config-version.cmake"
		VERSION ${arg_PROJ_VERSION}
		COMPATIBILITY SameMajorVersion
	)

	install(TARGETS ${FULL_PROJ_NAME} EXPORT ${FULL_PROJ_NAME}_targets)

	install(
		EXPORT ${FULL_PROJ_NAME}_targets
		NAMESPACE "${NAMESPACE}::"
		DESTINATION "${CMAKE_INSTALL_DATADIR}/${FULL_PROJ_NAME}"
	)

	install(
		FILES
			"${CMAKE_CURRENT_BINARY_DIR}/${FULL_PROJ_NAME}-config.cmake"
			"${CMAKE_CURRENT_BINARY_DIR}/${FULL_PROJ_NAME}-config-version.cmake"
		DESTINATION
			"${CMAKE_INSTALL_DATADIR}/${FULL_PROJ_NAME}"
	)

	if (DEFINED arg_PROJ_INCLUDE_DIRS)
		# IN LISTS: quoting the list expanded it to a single "a;b" item
		foreach (INCLUDE_DIR_ITER IN LISTS arg_PROJ_INCLUDE_DIRS)
			install(DIRECTORY "${INCLUDE_DIR_ITER}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
		endforeach()
	endif()

endfunction()
