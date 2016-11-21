# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=default
comment=comment
stringdq=string_1
stringlq=string_1
stringrq=string_1
command=function
parameters=parameter
variable=identifier_1
userdefined=type
whiledef=keyword_1
foreachdef=keyword_1
ifdefinedef=keyword_1
macrodef=preprocessor
stringvar=string_2
number=number_1

[keywords]
# all items must be in one line
commands=add_compile_options add_custom_command add_custom_target add_definitions add_dependencies add_executable add_library add_subdirectory add_test aux_source_directory break build_command cmake_host_system_information cmake_minimum_required cmake_policy configure_file create_test_sourcelist define_property elseif else enable_language enable_testing endforeach endfunction endif endmacro endwhile execute_process export file find_file find_library find_package find_path find_program fltk_wrap_ui foreach function get_cmake_property get_directory_property get_filename_component get_property get_source_file_property get_target_property get_test_property if include_directories include_external_msproject include_regular_expression include install link_directories list load_cache load_command macro mark_as_advanced math message option project qt_wrap_cpp qt_wrap_ui remove_definitions return separate_arguments set_directory_properties set_property set set_source_files_properties set_target_properties set_tests_properties site_name source_group string target_compile_definitions target_compile_options target_include_directories target_link_libraries try_compile try_run unset variable_watch while build_name exec_program export_library_dependencies install_files install_programs install_targets link_libraries make_directory output_required_files remove subdir_depends subdirs use_mangled_mesa utility_source variable_requires write_file
parameters=ABSOLUTE ABSTRACT ADDITIONAL_MAKE_CLEAN_FILES ALL AND APPEND APPLE ARGS ASCII BEFORE BORLAND CACHE CACHE_VARIABLES CLEAR CMAKE_COMPILER_2005 COMMAND COMMAND_NAME COMMANDS COMMENT COMPARE COMPILE_FLAGS COPYONLY CYGWIN DEFINED DEFINE_SYMBOL DEPENDS DOC EQUAL ESCAPE_QUOTES EXCLUDE EXCLUDE_FROM_ALL EXISTS EXPORT_MACRO EXT EXTRA_INCLUDE FATAL_ERROR FILE FILES FORCE FUNCTION GENERATED GLOB GLOB_RECURSE GREATER GROUP_SIZE HEADER_FILE_ONLY HEADER_LOCATION IMMEDIATE INCLUDE_DIRECTORIES INCLUDE_INTERNALS INCLUDE_REGULAR_EXPRESSION INCLUDES INTERFACE LESS LINK_DIRECTORIES LINK_FLAGS LOCATION MACOSX_BUNDLE MACROS MAIN_DEPENDENCY MAKE_DIRECTORY MATCH MATCHALL MATCHES MINGW MODULE MSVC MSVC60 MSVC70 MSVC71 MSVC80 MSVC_IDE MSYS NAME NAME_WE NO_SYSTEM_PATH NOT NOTEQUAL OBJECT_DEPENDS OFF ON OPTIONAL OR OUTPUT OUTPUT_VARIABLE PATH PATHS POST_BUILD POST_INSTALL_SCRIPT PRE_BUILD PREFIX PRE_INSTALL_SCRIPT PRE_LINK PREORDER PRIVATE PROGRAM PROGRAM_ARGS PROPERTIES PUBLIC QUIET RANGE READ REGEX REGULAR_EXPRESSION REPLACE REQUIRED RETURN_VALUE RUNTIME_DIRECTORY SEND_ERROR SHARED SOURCES STATIC STATUS STREQUAL STRGREATER STRLESS SUFFIX TARGET TOLOWER TOUPPER VAR VARIABLES VERSION WATCOM WIN32 WRAP_EXCLUDE WRITE
userdefined=


[settings]
# default extension used when saving files
extension=cmake

# MIME type
mime_type=text/x-cmake

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=#
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1
