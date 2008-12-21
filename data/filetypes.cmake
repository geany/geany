# For complete documentation of this file, please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false;
comment=0x808080;0xffffff;false;false;
stringdq=0xff901e;0xffffff;false;false;
stringlq=0x008000;0xffffff;false;false;
stringrq=0x008000;0xffffff;false;false;
command=0x00007f;0xffffff;false;false;
parameters=0x991111;0xffffff;false;false;
variable=0x007f7f;0xffffff;false;false;
userdefined=0x0000d0;0xffffff;true;false;
whiledef=0x00007f;0xffffff;true;false;
foreachdef=0x00007f;0xffffff;true;false;
ifdefinedef=0x00007f;0xffffff;true;false;
macrodef=0x00007f;0xffffff;true;false;
stringvar=0x007f7f;0xffffff;false;false;
number=0x007f00;0xffffff;false;false;

[keywords]
# all items must be in one line
commands=add_custom_command add_custom_target add_definitions add_dependencies add_executable add_library add_subdirectory add_test aux_source_directory build_command build_name cmake_minimum_required configure_file create_test_sourcelist else elseif enable_language enable_testing endforeach endif endmacro endwhile exec_program execute_process export_library_dependencies file find_file find_library find_package find_path find_program fltk_wrap_ui foreach get_cmake_property get_directory_property get_filename_component get_source_file_property get_target_property get_test_property if include include_directories include_external_msproject include_regular_expression install install_files install_programs install_targets link_directories link_libraries list load_cache load_command macro make_directory mark_as_advanced math message option output_required_files project qt_wrap_cpp qt_wrap_ui remove remove_definitions separate_arguments set set_directory_properties set_source_files_properties set_target_properties set_tests_properties site_name source_group string subdir_depends subdirs target_link_libraries try_compile try_run use_mangled_mesa utility_source variable_requires vtk_make_instantiator vtk_wrap_java vtk_wrap_python vtk_wrap_tcl while write_file
parameters=ABSOLUTE ABSTRACT ADDITIONAL_MAKE_CLEAN_FILES ALL AND APPEND APPLE ARGS ASCII BEFORE BORLAND CACHE CACHE_VARIABLES CLEAR CMAKE_COMPILER_2005 COMMAND COMMAND_NAME COMMANDS COMMENT COMPARE COMPILE_FLAGS COPYONLY CYGWIN DEFINED DEFINE_SYMBOL DEPENDS DOC EQUAL ESCAPE_QUOTES EXCLUDE EXCLUDE_FROM_ALL EXISTS EXPORT_MACRO EXT EXTRA_INCLUDE FATAL_ERROR FILE FILES FORCE FUNCTION GENERATED GLOB GLOB_RECURSE GREATER GROUP_SIZE HEADER_FILE_ONLY HEADER_LOCATION IMMEDIATE INCLUDE_DIRECTORIES INCLUDE_INTERNALS INCLUDE_REGULAR_EXPRESSION INCLUDES LESS LINK_DIRECTORIES LINK_FLAGS LOCATION MACOSX_BUNDLE MACROS MAIN_DEPENDENCY MAKE_DIRECTORY MATCH MATCHALL MATCHES MINGW MODULE MSVC MSVC60 MSVC70 MSVC71 MSVC80 MSVC_IDE MSYS NAME NAME_WE NO_SYSTEM_PATH NOT NOTEQUAL OBJECT_DEPENDS OFF ON OPTIONAL OR OUTPUT OUTPUT_VARIABLE PATH PATHS POST_BUILD POST_INSTALL_SCRIPT PRE_BUILD PREFIX PRE_INSTALL_SCRIPT PRE_LINK PREORDER PROGRAM PROGRAM_ARGS PROPERTIES QUIET RANGE READ REGEX REGULAR_EXPRESSION REPLACE REQUIRED RETURN_VALUE RUNTIME_DIRECTORY SEND_ERROR SHARED SOURCES STATIC STATUS STREQUAL STRGREATER STRLESS SUFFIX TARGET TOLOWER TOUPPER VAR VARIABLES VERSION WATCOM WIN32 WRAP_EXCLUDE WRITE
userdefined=


[settings]
# default extension used when saving files
#extension=cmake

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=
