[styling]
# Edit these in the colorscheme .conf file instead
default=default
key_major=keyword_1
key_minor=keyword_2
key_directive=keyword_1
comment_block=comment
comment_line=comment
comment_key=comment
comment_key_error=comment
identifier=identifier
variable=default
anonymous=default
number=number_1
operator=operator
string=string_1
string_quote=string_1
string_escape=string_1
string_escape_error=error
string_eol=string_1
embedded=string_1
placeholder=string_1

[keywords]
# all items must be in one line
# From https://github.com/arthwang/vsc-prolog and https://github.com/mxw/vim-prolog
primary=abolish abolish_op abolish_record abort abs absolute_file_name accept access_file acos acyclic_term add_attribute alarm als append append_strings apply arg argc argv arithmetic_function arity array array_concat array_flat array_list asin assert asserta assertz at at_end_of_stream at_eof atan atom atom_chars atom_codes atom_concat atom_length atom_number atom_prefix atom_string atom_to_term atomic attach_suspensions attached_suspensions autoload autoload_tool b_external bag_abolish bag_count bag_create bag_dissolve bag_enter bag_erase bag_retrieve bagof begin_module between bind block breal breal_bounds breal_from_bounds breal_max breal_min byte_count bytes_to_term call call-2 call_c call_cleanup call_explicit call_priority call_with_depth_limit callable cancel_after_event canonical_path_name catch cd ceil ceiling char_code char_conversion char_int char_type character_count chdir clause clause_property close clrbit code_type collation_key comment compare compare_instances compile compile_predicates compile_stream compile_term compile_term_annotated compiled_stream compound concat_atom concat_atoms concat_string concat_strings connect copy_stream_data copy_term copy_term_vars coroutine cos coverof cputime create_module current_after_event current_after_events current_arithmetic_function current_array current_atom current_blob current_built_in current_char_conversion current_compiled_file current_domain current_error current_flag current_format_predicate current_functor current_interrupt current_key current_macro current_module current_module_predicate current_op current_ouput current_pragma current_predicate current_record current_signal current_store current_stream current_struct current_suspension current_trigger cyclic_term date date_time_stamp date_time_value dbgcomp debug debug_reset debugging decval define_error define_macro delay delayed_goals delayed_goals_number delete delete_file demon denominator deprecated dim discontiguous display div domain domain_index downcase_atom duplicate_term dwim_match dwim_predicate dynamic e elif ensure_loaded enter_suspension_list env erase erase_all erase_array erase_macro erase_module errno_id error_id eval event event_after event_after_every event_create event_disable event_enable event_retrieve events_after events_defer events_nodefer exec exec_group existing_file exists exists_directory exists_file exit exit_block exp expand_clause expand_file_name expand_goal expand_macros external fail fail_if false file_base_name file_directory_name file_name_extension fileerrors findall fix flag flatten_array float float_fractional_part float_integer_part floor flush flush_output forall fork format format_predicate format_time frandom functor garbage_collect garbage_collect_atoms gcd get get0 get_byte get_char get_chtab get_code get_error_handler get_event_handler get_file_info get_flag get_interrupt_handler get_leash get_module_info get_priority get_prompt get_single_char get_stream get_stream_info get_suspension_data get_time get_timer get_var_bounds get_var_info getbit getcwd getenv getval global global_op ground halt hash help ignore import incval index init_suspension_list inline insert_suspension instance integer integer_atom is is_absolute_file_name is_array is_built_in is_dynamic is_event is_handle is_list is_locked is_predicate is_record is_stream is_suspension join_string keysort kill kill_display_matrix kill_suspension lcm length lib line_count line_position listen listing ln load local local_record local_time local_time_string locale_sort lock lock_pass log lsb macro make make_array make_directory make_display_matrix make_local_array make_suspension maplist max memberchk merge merge_set merge_suspension_lists message_hook message_to_string meta meta_attribute meta_bind meta_predicate min mkdir mod mode module module_interface msb msort mutex mutex_init name nb_linkarg nb_setarg new_socket_server nl nodbgcomp nodebug nonground nonvar normalize_space nospy not not_unify notify_constrained notrace nth_clause number number_chars number_codes number_merge number_sort number_string numbervars numerator occurs on_signal once op open open_null_stream os_file_name parallel parse_time pathname pause peek_byte peek_char peek_code peer peer_deregister_multitask peer_do_multitask peer_get_property peer_multitask_confirm peer_multitask_terminate peer_queue_close peer_queue_create peer_queue_get_property peer_register_multitask phrase pi pipe plus popcount portray portray_clause portray_goal portray_term powm pragma pred predicate_property predsort print print_message print_message_lines printf profile prolog_to_os_filename prompt prompt1 prune_instances put put_byte put_char put_code qcompile random rational rationalize rdiv read read_annotated read_clause read_directory read_exdr read_history read_link read_pending_input read_string read_term read_token readvar real record record_create recorda recorded recorded_list recordz redefine_system_predicate reexport reference referenced_record rem remote_connect remote_connect_accept remote_connect_setup remote_disconnect remote_yield rename rename_file repeat rerecord reset_error_handler reset_error_handlers reset_event_handler retract retract_all retractall round same_file same_term schedule_suspensions schedule_woken see seed seeing seek seen select set_chtab set_error_handler set_event_handler set_flag set_input set_interrupt_handler set_leash set_output set_prolog_IO set_prompt set_stream set_stream_position set_stream_property set_suspension_data set_suspension_priority set_timer set_tty set_var_bounds setarg setbit setenv setlocale setof setup_and_call_cleanup setval sgn sh shelf shelf_abolish shelf_create shelf_dec shelf_get shelf_inc shelf_set shell sin size_file skip skipped sleep socket sort split_string sprintf spy spy_term spy_var sqrt stack_parameter stamp_date_time statistics store store_contains store_count store_create store_delete store_erase store_get store_inc store_set stored_keys stored_keys_and_values stream_position_data stream_property stream_select stream_truncate string string_code string_concat string_length string_list string_to_atom string_to_list struct sub_atom sub_string subcall subscript substring subsumes subsumes_chk succ suffix sum suspend suspension_to_goal suspensions swritef system tab tan tell telling term_hash term_string term_to_atom term_to_bytes term_variables test_and_setval throw time time_file times tmp_file told tool tool_body trace trace_call_port trace_exit_port trace_parent_port trace_point_port traceable trigger trim_stacks trimcore true truncate tty_get_capability tty_goto tty_put tty_size ttyflush tyi tyo type_of unget unifiable unify_with_occurs_check unix unlock unschedule_suspension unsetenv unskipped untraceable upcase_atom update_struct use_module var variable variant wait wait_for_input wake wildcard_match win_exec win_folder win_has_menu win_insert_menu win_insert_menu_item win_registry_get_value win_shell win_window_pos window_title with_output_to working_directory write write_canonical write_exdr write_term writeclause writef writeln writeq xget xor xset yield
# Visual Prolog keywords: https://wiki.visual-prolog.com/index.php?title=Language_Reference/Built-in_entities#Predicates
secondary=and anyflow apicall bininclude bound c class class_name clauses constant_name constants constructors convert delegate determ digits digitsOf do domains else elseif endif erroneous error errorExit export externally fact_address facts failure finally foreach free from fromEllipsis goal guard hasDomain if implement in include inherits interface isErroneous language lowerBound maxDigits message monitor multi namespace nondeterm options or orelse orrequires otherwise predicate_fullname predicate_name predicates procedure programPoint prolog properties requires resolve retractFactDb sizeBitsOf sizeOf sizeOfDomain sourcefile_lineno sourcefile_name sourcefile_timestamp stdcall succeed supports then thiscall toAny toBinary toBoolean toEllipsis toString toTerm trap try tryToTerm tryConvert typeDescriptorOf typeLibraryOf uncheckedConvert upperBound

[lexer_properties]
lexer.visualprolog.verbatim.strings=0
lexer.visualprolog.backquoted.strings=1

[settings]
# default extension used when saving files
extension=pro

# MIME type
mime_type=text/x-prolog

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=%
# multiline comments
#comment_open=/*
#comment_close=*/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
# 		#command_example();
# setting to false would generate this
# #		command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=swipl -c "%f"

run_cmd=swipl -q -s "%f"

# Parse syntax check error messages and warnings, examples:
# ERROR: goo.pro:12:
error_regex=.+: (.+):([0-9]+).*
