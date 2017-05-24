/*
 * geany.vapi - This file comes from the Geany Vala Binding project
 *
 * Copyright 2010-2012 Colomban Wendling <ban(at)herbesfolles(dot)org>
 * Copyright 2012 Matthew Brush <matt@geany.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

[CCode (cheader_filename = "geanyplugin.h")]
namespace Geany {
	/* reviewed */
	[Compact]
	[CCode (cname = "GeanyApp")]
	public class Application {
		public bool								debug_mode;
		[CCode (cname = "configdir")]
		public string							config_dir;
		[CCode (cname = "datadir")]
		public string							data_dir;
		[CCode (cname = "docdir")]
		public string							doc_dir;
		public unowned TagManager.Workspace		tm_workspace;
		public Project?							project;
	}
	/* reviewed */
	[Compact]
	public class InterfacePrefs {
		public bool		sidebar_symbol_visible;
		public bool		sidebar_openfiles_visible;
		public string	editor_font;
		public string	tagbar_font;
		public string	msgwin_font;
		public bool		show_notebook_tabs;
		public int		tab_pos_editor;
		public int		tab_pos_msgwin;
		public int		tab_pos_sidebar;
		public bool		statusbar_visible;
		public bool		show_symbol_list_expanders;
		public bool		notebook_double_click_hides_widgets;
		public bool		highlighting_invert_all;
		public int		sidebar_pos;
		public bool		msgwin_status_visible;
		public bool		msgwin_compiler_visible;
		public bool		msgwin_messages_visible;
		public bool		msgwin_scribble_visible;
		public bool		use_native_windows_dialogs; /* only used on Windows */
		public bool		compiler_tab_autoscroll;
	}
	/* reviewed */
	/**
	 * Indentation prefs that might be different according to the
	 * {@link Project} or {@link Document}.
	 * 
	 * Use {@link Editor.indent_prefs} to get the prefs for a
	 * particular document.
	 **/
	[Compact]
	public class IndentPrefs {
		/**
		 * The indentation with, in character size.
		 * 
		 * This is not necessarily the number of character used for indentation
		 * if e.g. indentation uses tabs or tabs+space.
		 */
		public int width {
			[CCode (cname = "geany_vala_pluing_indent_prefs_get_width")]
			get { return _width; }
		}
		[CCode (cname = "width")] int _width;
		/**
		 * The indentation type (spaces, tabs or both).
		 */
		public IndentType type {
			[CCode (cname = "geany_vala_pluing_indent_prefs_get_type")]
			get { return _type; }
		}
		[CCode (cname = "type")] IndentType _type;
		/**
		 * The natural width of a tab, used when {@link type} is
		 * {@link IndentType.BOTH}.
		 * 
		 * @see Sci.get_tab_width
		 */
		public int hard_tab_width {
			[CCode (cname = "geany_vala_pluing_indent_prefs_get_hard_tab_width")]
			get { return _hard_tab_width; }
		}
		[CCode (cname = "hard_tab_width")] int _hard_tab_width;
		/**
		 * Mode of auto-indentation
		 */
		public AutoIndent auto_indent_mode {
			[CCode (cname = "geany_vala_pluing_indent_prefs_get_auto_indent_mode")]
			get { return _auto_indent_mode; }
		}
		[CCode (cname = "auto_indent_mode")] AutoIndent _auto_indent_mode;
		/**
		 * Whether to try to detect automatically or not the indentation type
		 */
		public bool detect_type {
			[CCode (cname = "geany_vala_pluing_indent_prefs_get_detect_type")]
			get { return _detect_type; }
		}
		[CCode (cname = "detect_type")] bool _detect_type;
		
		/**
		 * Gets the default indentation prefs.
		 * 
		 * Prefs can be different according to the {@link Project} or 
		 * {@link Document}.
		 * 
		 * ''Warning:'' Always get a fresh result instead of keeping a 
		 * reference around if the editor/project settings may have changed,
		 * or if this function has been called for a different editor.
		 * 
		 * If you have an editor and want to get its indent prefs, use
		 * {@link Editor.indent_prefs}.
		 * 
		 * @param editor Don't use this parameter
		 * @return The default indentation preferences
		 * 
		 * @see Editor.indent_prefs
		 */
		[CCode (cname = "editor_get_indent_prefs")]
		public static unowned IndentPrefs get_default (Editor? editor = null);
	}
	/* reviewed */
	[Compact]
	public class EditorPrefs {
		public IndentPrefs	indentation;
		public bool			show_white_space;
		public bool			show_indent_guide;
		public bool			show_line_endings;
		public int			long_line_type;
		public int			long_line_column;
		public string		long_line_color;
		public bool			show_markers_margin;		/* view menu */
		public bool			show_linenumber_margin;		/* view menu */
		public bool			show_scrollbars;			/* hidden pref */
		public bool			scroll_stop_at_last_line;
		public bool			line_wrapping;
		public bool			use_indicators;
		public bool			folding;
		public bool			unfold_all_children;
		public bool			disable_dnd;
		public bool			use_tab_to_indent;	/* makes tab key indent instead of insert a tab char */
		public bool			smart_home_key;
		public bool			newline_strip;
		public bool			auto_complete_symbols;
		public bool			auto_close_xml_tags;
		public bool			complete_snippets;
		public int			symbolcompletion_min_chars;
		public int			symbolcompletion_max_height;
		public bool			brace_match_ltgt;	/* whether to highlight < and > chars (hidden pref) */
		public bool			use_gtk_word_boundaries;	/* hidden pref */
		public bool			complete_snippets_whilst_editing;	/* hidden pref */
		public int			line_break_column;
		public bool			auto_continue_multiline;
		public string		comment_toggle_mark;
		public uint			autocompletion_max_entries;
		public uint			autoclose_chars;
		public bool			autocomplete_doc_words;
		public bool			completion_drops_rest_of_word;
		public string		color_scheme;
		public int			show_virtual_space;
		public bool			long_line_enabled;
		
		/**
		 * Gets the default End of Line character (LF, CR/LF, CR), one of "\n",
		 * "\r\n" or "\r".
		 * 
		 * If you have an editor and want to know it's EOL character, use
		 * {@link Editor.eol_char}.
		 * 
		 * @param editor Don't use this parameter
		 * @return The default EOL character
		 * 
		 * @see Editor.eol_char
		 */
		[CCode (cname = "editor_get_eol_char")]
		public static unowned string get_default_eol_char (Editor? editor = null);
		
		/**
		 * Gets the default end of line characters mode.
		 * 
		 * If you have an editor and want to know it's EOL mode, use
		 * {@link Editor.eol_char_mode}.
		 * 
		 * @param editor Don't use this parameter
		 * @return The default EOL mode
		 * 
		 * @see Editor.eol_char_mode
		 */
		[CCode (cname = "editor_get_eol_char_mode")]
		public static Scintilla.EolMode get_default_eol_char_mode (Editor? editor = null);
		
		/**
		 * Gets the localized name (for displaying) of the default End of Line
		 * characters (LF, CR/LF, CR).
		 * 
		 * If you have an editor and want to know it's EOL char name, use
		 * {@link Editor.eol_char_name}.
		 * 
		 * @param editor Don't use this parameter
		 * @return The default EOL character's name
		 * 
		 * @see Editor.eol_char_name
		 */
		[CCode (cname = "editor_get_eol_char_name")]
		public static unowned string get_default_eol_char_name (Editor? editor = null);
	}
	/* reviewed */
	[Compact]
	public class ToolbarPrefs {
		public bool				visible;
		public Gtk.IconSize		icon_size;
		public Gtk.ToolbarStyle	icon_style;
		public bool				use_gtk_default_style;
		public bool				use_gtk_default_icon;
		public bool				append_to_menu;
	}
	/* reviewed */
	[Compact]
	public class Prefs {
		public bool		load_session;
		public bool		load_plugins;
		public bool		save_winpos;
		public bool		confirm_exit;
		public bool		beep_on_errors;		/* use utils_beep() instead */
		public bool		suppress_status_messages;
		public bool		switch_to_status;
		public bool		auto_focus;
		public string	default_open_path;
		public string	custom_plugin_path;
	}
	/* reviewed */
	[Compact]
	public class ToolPrefs {
		public string	browser_cmd;
		public string	term_cmd;
		public string	grep_cmd;
		public string	context_action_cmd;
	}
	/* reviewed */
	[CCode (cname = "GeanyFindSelOptions", cprefix = "GEANY_FIND_SEL_", has_type_id = false)]
	public enum FindSelectionType {
		CURRENT_WORD,
		SEL_X,
		AGAIN
	}
	/* reviewed */
	[Compact]
	public class SearchPrefs {
		public bool					suppress_dialogs;
		public bool					use_current_word;
		public bool					use_current_file_dir;
		public FindSelectionType	find_selection_type;
	}
	/* reviewed
	 * FIXME: are (some of) these nullable? */
	[Compact]
	public class TemplatePrefs {
		public string	developer;
		public string	company;
		public string	mail;
		public string	initials;
		public string	version;
		public string	year_format;
		public string	date_format;
		public string	datetime_format;
	}
	/* reviewed */
	[Compact]
	public class FilePrefs {
		public int		default_new_encoding;
		public int		default_open_encoding;
		public bool		final_new_line;
		public bool		strip_trailing_spaces;
		public bool		replace_tabs;
		public bool		tab_order_ltr;
		public bool		tab_order_beside;
		public bool		show_tab_cross;
		public uint		mru_length;
		public int		default_eol_character;
		public int		disk_check_timeout;
		public bool		cmdline_new_files;
		public bool		use_safe_file_saving;
		public bool		ensure_convert_new_lines;
		public bool		gio_unsafe_save_backup;
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_GBG_", has_type_id = false)]
	public enum BuildGroup {
		FT,			/* *< filetype items */
		NON_FT,		/* *< non filetype items.*/
		EXEC,		/* *< execute items */
		COUNT		/* *< count of groups. */
	}
	/* reviewed */
	[Compact]
	public class BuildInfo {
		[CCode (cname = "grp")]
		BuildGroup		group;
		[CCode (cname = "cmd")]
		int				command;
		GLib.Pid		pid;	/* id of the spawned process */
		string			dir;
		uint			file_type_id;
		string			custom_target;
		int				message_count;
	}
	/* reviewed */
	[Compact]
	public class Data {
		public Application				app;
		public MainWidgets				main_widgets;
		public GLib.PtrArray			documents_array;
		public GLib.PtrArray			filetypes_array;
		public Prefs					prefs;
		public InterfacePrefs			interface_prefs;
		public ToolbarPrefs				toolbar_prefs;
		public EditorPrefs				editor_prefs;
		public FilePrefs				file_prefs;
		public SearchPrefs				search_prefs;
		public ToolPrefs				tool_prefs;
		public TemplatePrefs			template_prefs;
		public BuildInfo				build_info;
		public GLib.SList<Filetype>		filetypes_by_title;
	}
	/* reviewed */
	[CCode (cprefix = "dialogs_")]
	namespace Dialogs {
		public string?	show_input (string title, Gtk.Window parent,
									string? label_text = null, string? default_text = null);
		public bool		show_input_numeric (string title, string label_text,
											ref double value, double min, double max, double step);
		[PrintfFormat]
		public void		show_msgbox (Gtk.MessageType type, string text, ...);
		[PrintfFormat]
		public bool		show_question (string text, ...);
		public bool		show_save_as ();
	}
	/* reviewed */
	[Compact]
	[CCode (cname = "struct GeanyDocument",
			cprefix = "document_",
			free_function = "document_close")]
	public class Document {
		/* Private methods used to implement properties */

		private void						set_encoding (string new_encoding);
		private void						set_filetype (Filetype type);
		private void						set_text_changed (bool changed);
		private bool 						save_file_as (string? utf8_fname = null);

		/* Document Properties */

		/**
		 * Gets whether the document has changed since it was last saved.
		 */
		public bool has_changed {
			[CCode (cname = "geany_vala_plugin_document_get_has_changed")]
			get { return _has_changed; }
			[CCode (cname = "geany_vala_plugin_document_set_has_changed")]
			set { set_text_changed (value); }
		}
		[CCode (cname = "changed")] bool _has_changed;
		
		/**
		 * Gets the {@link Editor} associated with the document.
		 */
		public Editor editor {
			[CCode (cname = "geany_vala_plugin_document_get_editor")]
			get { return _editor; }
		}
		[CCode (cname = "editor")] Editor _editor;

		/**
		 * Gets/sets the encoding of the document. 
		 * 
		 * This must be a valid string representation of an encoding, which
		 * can be retrieved with the {@link Encoding.get_charset_from_index}
		 * function.
		 */
		public string encoding {
			[CCode (cname = "geany_vala_plugin_document_get_encoding")]
			get { return _encoding; }
			[CCode (cname = "geany_vala_plugin_document_set_encoding")]
			set { set_encoding (value); }
		}
		[CCode (cname = "encoding")] string _encoding;
		
		/**
		 * Gets the document's file name or a string representing an unsaved
		 * document, for example 'untitled'.
		 */
		/* FIXME: I'm not really sure of the name of this property */
		public string safe_file_name {
			[CCode (cname = "DOC_FILENAME")] get;
		}
		
		/**
		 * Gets/sets this document's UTF-8 encoded filename. 
		 * 
		 * Setting this property has the same effect as using the 
		 * {@link save_as} method.
		 */
		public string? file_name {
			[CCode (cname = "geany_vala_plugin_document_get_file_name")]
			get { return _file_name; }
		}
		[CCode (cname = "file_name")] string? _file_name;
		
		/**
		 * Gets/sets the filetype for this document, which controls syntax
		 * highlighting and tags.
		 */
		public Filetype file_type {
			[CCode (cname = "geany_vala_plugin_document_get_file_type")]
			get { return _file_type; }
			[CCode (cname = "geany_vala_plugin_document_set_file_type")]
			set { set_filetype (value); }
		}
		[CCode (cname = "file_type")] Filetype _file_type;
		
		/**
		 * Gets whether the file for this document has a Byte Order Mark.
		 */
		public bool has_bom {
			[CCode (cname = "geany_vala_plugin_document_get_has_bom")]
			get { return _has_bom; }
		}
		[CCode (cname = "has_bom")] bool _has_bom;
		
		/**
		 * Gets whether this document supports source code symbols (tags).
		 * 
		 * Tags will show in the sidebar and will be used for auto-completion
		 * and calltips.
		 */
		public bool has_tags {
			[CCode (cname = "geany_vala_plugin_document_get_has_tags")]
			get { return _has_tags; }
		}
		[CCode (cname = "has_tags")] bool _has_tags;
		
		/**
		 * Gets the document's index in the documents array.
		 */
		/* Not sure why indes is signed in Geany's side, it is always >= 0 and
		 * represents a array index. However, it would need to change this
		 * get_from_index() too, so wrap it... */
		public int index {
			[CCode (cname = "geany_vala_plugin_document_get_index")]
			get { return _index; }
		}
		[CCode (cname = "index")] int _index;
		
		/**
		 * Gets whether the document is active and all properties are set
		 * correctly.
		 */
		public bool is_valid { [CCode (cname = "DOC_VALID")] get; }
		
		/**
		 * Gets/sets whether this document is read-only.
		 */
		public bool is_read_only {
			[CCode (cname = "geany_vala_plugin_document_get_is_read_only")]
			get { return _is_read_only; }
		}
		[CCode (cname = "readonly")] bool _is_read_only;
		
		
		/**
		 * Gets the link-dereferenced, local-encoded file name for this 
		 * {@link Document}. 
		 * 
		 * The value can be null if the document is not saved to disk yet 
		 * (has no file).
		 */
		public string? real_path {
			[CCode (cname = "geany_vala_plugin_document_get_real_path")]
			get { return _real_path; }
		}
		[CCode (cname = "real_path")] string? _real_path;
		
		/**
		 * Gets the {@link TagManager.WorkObject} used for this document.
		 * 
		 * The value can be null if there is no tag manager used for this 
		 * document.
		 */
		public TagManager.WorkObject? tm_file { /* needs better name? */
			[CCode (cname = "geany_vala_plugin_document_get_tm_file")]
			get { return _tm_file; }
		}
		[CCode (cname = "tm_file")] TagManager.WorkObject? _tm_file;
		
		/**
		 * Gets the {@link Gtk.Notebook} page index for this document.
		 */
		public int notebook_page { /* should cast to uint? */
			[CCode (cname = "document_get_notebook_page")] get;
		}
		
		/**
		 * Gets the status color of the document or null if the default
		 * widget coloring should be used.
		 */
		public unowned Gdk.Color? status_color {
			[CCode (cname = "document_get_status_color")] get;
		}
		
		/* Document Methods */
		
		/**
		 * Gets the last part of the filename for this document, ellipsized 
		 * (shortened) to the desired //length// or a default value if 
		 * //length// is -1.
		 * 
		 * @param length The number of characters to ellipsize the basename to.
		 *
		 * @return The displayable basename for this document's file.
		 */
		[CCode (cname = "document_get_basename_for_display")]
		public string get_display_basename (int length = -1);
		
		/**
		 * Opens each file in the list //filenames// using the specified 
		 * settings.
		 * 
		 * @param filenames  A list of filenames to load, in locale encoding.
		 * @param readonly   Whether to open the document in read-only mode.
		 * @param ft         The filetype for the document or null to 
		 *                   automatically detect the filetypes.
		 * @param forced_enc The file encoding to use or null to 
		 *                   automatically detect the file encoding. 
		 */
		public static void open_files (GLib.SList<string> filenames,
									   bool readonly = false,
									   Filetype? ft = null,
									   string? forced_enc = null);

		/**
		 * Reloads the document's file using the specified //forced_enc//
		 * encoding.
		 * 
		 * @param forced_enc The file encoding to reload with or null to 
		 *                   detect the encoding automatically.
		 * 
		 * @return true if the document was reloaded successfully, otherwise
		 *         false is returned.
		 */
		[CCode (cname = "document_reload_file")]
		public bool reload (string? forced_enc = null);
		
		/**
		 * Renames this documents file to //new_filename//.
		 * 
		 * Only the file on disk is actually renamed, you still have to call 
		 * {@link save_as} to update this document. This function also stops
		 * monitoring for file changes to prevent receiving too many file 
		 * change events while renaming. File monitoring is setup again in 
		 * {@link save_as}.
		 * 
		 * @param new_filename The new filename in UTF-8 encoding.
		 * 
		 * @see save_as
		 */
		[CCode (cname = "document_rename_file")]
		public void rename (string new_filename);
		
		/**
		 * Saves this document to file.
		 * 
		 * Saving may include replacing tab with spaces, stripping trailing
		 * spaces and adding a final newline at the end of the file, depending
		 * on user preferences. Next, the "document-before-save" signal is
		 * emitted, allowing plugins to modify the document before it is saved,
		 * and data is actually written to disk. The filetype is set again or
		 * auto-detected if it hasn't been set yet. After this, the 
		 * "document-save" signal is emitted for plugins.
		 * 
		 * If the file has not been modified, this functions does nothing
		 * unless //force// is set to true.
		 * 
		 * You should ensure {@link file_name} is not null before calling
		 * this, otherwise you should call {@link Dialogs.show_save_as}.
		 * 
		 * @param force Whether to save the file even if it is not modified.
		 * 
		 * @return true if the file was saved or false if the file could not
		 *         or should not be saved.
		 * 
		 * @see Dialogs.show_save_as
		 */
		[CCode (cname = "document_save_file")]
		public bool save (bool force = false);
		
		/**
		 * Saves the document, detecting filetype.
		 * 
		 * @param utf8_fname The new name for the document, in UTF-8 
		 *                   encoding or null.
		 * 
		 * @return true if the file was saved or false if the file could
		 *         not be saved.
		 */
		[CCode (cname = "document_save_file_as")]
		public bool save_as (string? utf8_fname = null); /* what does null do? */
		
		/* Document Construction, Retrieval and Destruction Methods */
		
		/**
		 * Creates a new document.
		 * 
		 * Line endings in //text// will be converted to the default setting.
		 * Afterwards, the "document-new" signal is emitted for plugins.
		 * 
		 * @param utf8_filename The file name in UTF-8 encoding or null to
		 *                      open a file as "untitled".
		 * @param ft            Filetype to set or null to detect it from
		 *                      //utf8_filename// if not null.
		 * @param text          The initial content of the file (in UTF-8
		 *                      encoding) or null.
		 * 
		 * @return A new {@link Document}.
		 */
		[CCode (cname = "document_new_file")]
		public static unowned Document? @new (string? utf8_filename = null,
											  Filetype? ft = null,
											  string? text = null);
		
		/**
		 * Closes this document.
		 * 
		 * @return true if the document was actually removed or false otherwise.
		 */
		public bool close ();
		
		/**
		 * Removes the given {@link Gtk.Notebook} tab page at //page_num//
		 * and clears all related information in the documents list.
		 * 
		 * @param page_num The {@link Gtk.Notebook} tab page number to remove.
		 * 
		 * @return true if the document was actually removed or false otherwise.
		 */
		[CCode (cname = "document_remove_page")]
		public static bool close_notebook_page (uint page_num);
		
		/**
		 * Finds a document with the given filename.
		 * 
		 * This matches either the exact //utf8_filename// or variant
		 * filenames with relative elements in the path, eg. '/dir/../\/name' 
		 * will match '/name'.
		 * 
		 * @param utf8_filename The UTF-8 encoded filename to search for.
		 * 
		 * @return The matching {@link Document} or null if no document matched.
		 */
		 /* FIXME: double forward slash in valadoc comment. */
		public static unowned Document?	find_by_filename (string utf8_filename);
		
		/**
		 * Finds a document whose //real_name// matches the given filename.
		 * 
		 * @param real_name The filename to search, which should be identical
		 *                  to the string returned by 
		 *                  {@link TagManager.get_real_path}.
		 * 
		 * @return The matching {@link Document} or null if no document matched.
		 */
		public static unowned Document?	find_by_real_path (string real_name);
		
		/**
		 * Finds the current document.
		 * 
		 * @return The current {@link Document} or null if there are no opened
		 *         documents.
		 */
		public static unowned Document?	get_current ();
		
		/**
		 * Finds the document for the given notebook page number 
		 * //page_num//.
		 * 
		 * @param page_num The notebook page number to search.
		 * 
		 * @return The corresponding document for the given notebook page
		 *         or null if there was no document at //page_num//.
		 */
		[CCode (cname = "document_get_from_page")]
		public static unowned Document?	get_from_notebook_page (uint page_num);
		
		/**
		 * Gets the document from the documents array with //index//.
		 * 
		 * Always check the returned document is valid.
		 * 
		 * @param index Documents array index.
		 * 
		 * @return The document with //index// or null if //index// is
		 *         out of range.
		 */
		[CCode (cname = "document_index")]
		public static unowned Document?	get_from_index (int index);
		
		/**
		 * Opens a new document specified by //locale_filename//.
		 * 
		 * Afterwards, the "document-open" signals is emitted for plugins.
		 * 
		 * @param locale_filename The filename of the document to load, in 
		 *                        locale encoding.
		 * @param read_only       Whether to open the document in read-only
		 *                        mode.
		 * @param ft              The filetype for the document or null to
		 *                        auto-detect the filetype.
		 * @param forced_enc      The file encoding to use or null to
		 *                        auto-detect the file encoding.
		 * 
		 * @return The opened document or null.
		 */
		[CCode (cname = "document_open_file")]
		public static unowned Document?	open (string locale_filename,
											  bool readonly = false,
											  Filetype? ft = null, 
											  string? forced_enc = null);
	}
	/* reviewed */
	[CCode (cname = "documents", array_length_cexpr = "GEANY(documents_array)->len")]
	public static unowned Document[]	documents;
	/* reviewed */
	[CCode (cprefix = "GEANY_INDICATOR_", has_type_id = false)]
	public enum Indicator {
		ERROR,
		SEARCH
	}
	namespace Snippets {
		/**
		 * Gets a snippet by name.
		 * 
		 * If //editor// is not null, returns a snippet specific to the
		 * document filetype. If //editor// is null, returns a snippet from
		 * the default set.
		 * 
		 * @param editor {@link Editor} or null.
		 * @param snippet Name of snippet to get.
		 * 
		 * @return Snippet or null if it was not found.
		 * 
		 * @see Editor.find_snippet
		 */
		[CCode (cname = "editor_find_snippet")]
		public unowned string? find (Editor? editor, string snippet_name);
	}
	/* reviewed */
	[Compact]
	[CCode (cname = "struct GeanyEditor", cprefix = "editor_")]
	public class Editor {
		/* Private methods used to implement properties */

		private void set_indent_type (IndentType ind_type);
		
		[CCode (cname = "GEANY_WORDCHARS")]
		public static const string WORD_CHARS;
		
		/* Editor Properties */

		/**
		 * true if auto-indentation is enabled false otherwise.
		 */
		public bool auto_indent {
			[CCode (cname = "geany_vala_plugin_editor_get_auto_indent")]
			get { return _auto_indent; }
		}
		[CCode (cname = "auto_indent")] bool _auto_indent;
		
		/**
		 * The {@link Document} associated with the editor.
		 * 
		 * @see Document
		 */
		public Document document {
			[CCode (cname = "geany_vala_plugin_editor_get_document")]
			get { return _document; }
		}
		[CCode (cname = "document")] Document _document;
		
		/**
		 * The width for the indentation, in characters. This is not
		 * necessarily the number of character used for indentation if e.g.
		 * indentation uses tabs or tabs+space.
		 */
		public int indent_width {
			[CCode (cname = "geany_vala_plugin_editor_get_indent_width")]
			get { return this.indent_prefs.width; }
		}
		
		/**
		 * Whether long lines are split as you type or not.
		 */
		public bool line_breaking {
			[CCode (cname = "geany_vala_plugin_editor_get_line_breaking")]
			get { return _line_breaking; }
		}
		[CCode (cname = "line_breaking")] bool _line_breaking;
		
		/**
		 * true if line wrapping is enabled.
		 */
		public bool line_wrapping {
			[CCode (cname = "geany_vala_plugin_editor_get_line_wrapping")]
			get { return _line_wrapping; }
		}
		[CCode (cname = "line_wrapping")] bool _line_wrapping;
		
		/**
		 * The {@link Sci} [[http://www.scintilla.org/|Scintilla]] editor widget
		 * associated with this editor.
		 * 
		 * @see Sci
		 */
		public Sci scintilla {
			[CCode (cname = "geany_vala_plugin_editor_get_scintilla")]
			get { return _scintilla; }
		}
		[CCode (cname = "sci")] Sci _scintilla;
		
		/**
		 * The percentage to scroll view by when painting, if positive.
		 */
		public float scroll_percent {
			[CCode (cname = "geany_vala_plugin_editor_get_scroll_percent")]
			get { return _scroll_percent; }
		}
		[CCode (cname = "scroll_percent")] float _scroll_percent;
		
		/**
		 * Gets the used End of Line characters (LF, CR/LF, CR) in this
		 * {@link Editor}. The value will be one of "\n", "\r\n" or "\r".
		 * 
		 * @see EditorPrefs.get_default_eol_char
		 */
		public unowned string eol_char {
			[CCode (cname = "editor_get_eol_char")] get;
		}
		
		/**
		 * Gets the end of line characters mode in this {@link Editor}.
		 * 
		 * @see EditorPrefs.get_default_eol_char_mode
		 * @see Scintilla.EolMode
		 */
		public Scintilla.EolMode eol_char_mode {
			[CCode (cname = "editor_get_eol_char_mode")] get;
		}
		
		/**
		 * Retrieves the localized name (for displaying) of the used End of
		 * Line characters (LF, CR/LF, CR) in this {@link Editor}.
		 * 
		 * @see EditorPrefs.get_default_eol_char_name
		 */
		public unowned string eol_char_name {
			[CCode (cname = "editor_get_eol_char_name")] get;
		}
		
		/**
		 * Gets the indentation prefs for this {@link Editor}.
		 * 
		 * Prefs can be different according to the {@link Project} or 
		 * {@link Document}.
		 * 
		 * ''Warning:'' Always get a fresh result instead of keeping a 
		 * reference around if the editor/project settings may have changed,
		 * or if this function has been called for a different editor.
		 * 
		 * @see IndentPrefs.get_default
		 */
		public unowned IndentPrefs indent_prefs {
			[CCode (cname = "editor_get_indent_prefs")] get;
		}
		
		/**
		 * The indent type for the editor.
		 */
		public IndentType indent_type {
			[CCode (cname = "geany_vala_plugin_editor_get_indent_type")]
			get { return this.indent_prefs.type; }
			[CCode (cname = "geany_vala_plugin_editor_set_indent_type")]
			set { set_indent_type (value); }
		}
		
		/* Editor Methods */
		
		/**
		 * Gets a snippet by name for the language used by this {@link Editor}.
		 * 
		 * @param snippet Name of snippet to get.
		 * 
		 * @return Snippet or null if it was not found.
		 * 
		 * @see Snippets.find
		 */
		public unowned string? find_snippet (string snippet_name);
		
		/**
		 * Finds the word at the position specified by //pos//.
		 * 
		 * Additional //wordschars// can be specified to define what to 
		 * consider a word.
		 * 
		 * @param pos       The {@link Editor} to operate on.
		 * @param wordchars The characters that separate words, meaning all
		 *                  characters to count as part of a word. If this
		 *                  is null, the default {@link WORD_CHARS} are used.
		 * 
		 * @return A string containing the word at the given position or null.
		 */
		/* pos should be unsigned? */
		public string? get_word_at_pos (int pos, string? wordchars = null);
		
		/**
		 * Moves the position to //pos//, switching to the document if
		 * necessary, setting a marker if //mark// is true.
		 * 
		 * @param pos  The position to go to.
		 * @param mark Whether to set a mark on the position or not.
		 * 
		 * @return true if the action has been performed, false otherwise.
		 */
		/* pos should be unsigned? */
		public bool goto_pos (int pos, bool mark = false);
		
		/**
		 * Deletes all currently set indicators matching //indic// in this
		 * {@link Editor}'s window.
		 * 
		 * @param indic The {@link Indicator} to clear.
		 */
		public void indicator_clear (Indicator indic);
		
		/**
		 * Sets an indictor //indic// on //line//.
		 * 
		 * Whitespace at the start and end of the line is not marked.
		 * 
		 * @param indic The {@link Indicator} to use.
		 * @param line  The line number which should be marked.
		 * 
		 * @see indicator_set_on_range
		 */
		/* line should be unsigned? */
		public void indicator_set_on_line (Indicator indic, int line);
		
		/**
		 * Sets an indicator on the range specified by //start// and //end//.
		 * 
		 * No error checking or whitespace removal is performed, this should
		 * be done by the caller if necessary.
		 * 
		 * @param indic The @{link Indicator} to use.
		 * @param start The starting position for the marker.
		 * @param end   The ending position for the marker.
		 * 
		 * @see indicator_set_on_line
		 */
		public void indicator_set_on_range (Indicator indic, int start, int end);
		
		/**
		 * Replaces all special sequences in //snippet// and inserts it at
		 * //pos//.
		 * 
		 * If you insert at the current position, consider calling
		 * {@link Sci.scroll_caret} after this method.
		 * 
		 * @param pos     The position to insert the snippet at.
		 * @param snippet The snippet to use.
		 */
		/* pos should be unsigned? */
		public void insert_snippet (int pos, string snippet);
		
		/**
		 * Inserts text, replacing //\t// characters (0x9) and //\n// 
		 * characters accordingly for the document.
		 * 
		 *  * Leading tabs are replaced with the correct indentation.
		 *  * Non-leading tabs are replaced with spaces (except when using
		 *    'Tabs' indent type.
		 *  * Newline chars are replaced with the correct line ending string.
		 *    This is very useful for inserting code without having to handle
		 *    the indent type yourself (Tabs & Spaces mode can be tricky).
		 * 
		 * ''Warning:'' Make sure all //\t// tab chars in //text// are indented
		 * as indent widths or alignment, not hard tabs, as those won't be
		 * preserved.
		 * 
		 * ''Note:'' This method doesn't scroll the cursor into view afterwards.
		 * 
		 * @param text                Text to insert indented for example as 
		 *                            'if (foo)\n\tbar();'.
		 * @param insert_pos          Position to insert text at. 
		 * @param cursor_index        If >= 0, the index into //text// to place
		 *                            the cursor at.
		 * @param newline_indent_size Indentation size (in characters) to
		 *                            insert for each newline. Use -1 to read
		 *                            the indent size from the line with
		 *                            //insert_pos// on it.
		 * @param replace_newlines    Whether to replace newlines or not.  If
		 *                            newlines have been replaced already, this
		 *                            should be false, to avoid errors, for
		 *                            example on Windows.
		 */
		/* pos should be unsigned? */
		public void insert_text_block (string text, 
									   int insert_pos,
									   int cursor_index = -1,
									   int newline_indent_size = -1,
									   bool replace_newlines = true);
	}
	/* reviewed */
	[CCode (cname = "GeanyEncodingIndex", cprefix = "GEANY_ENCODING_", has_type_id = false)]
	public enum EncodingID {
		ISO_8859_1,
		ISO_8859_2,
		ISO_8859_3,
		ISO_8859_4,
		ISO_8859_5,
		ISO_8859_6,
		ISO_8859_7,
		ISO_8859_8,
		ISO_8859_8_I,
		ISO_8859_9,
		ISO_8859_10,
		ISO_8859_13,
		ISO_8859_14,
		ISO_8859_15,
		ISO_8859_16,

		UTF_7,
		UTF_8,
		UTF_16LE,
		UTF_16BE,
		UCS_2LE,
		UCS_2BE,
		UTF_32LE,
		UTF_32BE,

		ARMSCII_8,
		BIG5,
		BIG5_HKSCS,
		CP_866,

		EUC_JP,
		EUC_KR,
		EUC_TW,

		GB18030,
		GB2312,
		GBK,
		GEOSTD8,
		HZ,

		IBM_850,
		IBM_852,
		IBM_855,
		IBM_857,
		IBM_862,
		IBM_864,

		ISO_2022_JP,
		ISO_2022_KR,
		ISO_IR_111,
		JOHAB,
		KOI8_R,
		KOI8_U,

		SHIFT_JIS,
		TCVN,
		TIS_620,
		UHC,
		VISCII,

		WINDOWS_1250,
		WINDOWS_1251,
		WINDOWS_1252,
		WINDOWS_1253,
		WINDOWS_1254,
		WINDOWS_1255,
		WINDOWS_1256,
		WINDOWS_1257,
		WINDOWS_1258,

		NONE,
		CP_932
	}
	/* reviewed */
	[Compact]
	[CCode (cprefix = "encodings_")]
	public class Encoding {
		[CCode (cname = "idx")]
		public EncodingID				index;
		public unowned string			charset;
		public unowned string			name;
		
		public static string?			convert_to_utf8 (string buffer, size_t size,
														 out string used_encoding = null);
		public static string?			convert_to_utf8_from_charset (string buffer, size_t size,
																	  string charset, bool fast);
		public static unowned string?	get_charset_from_index (EncodingID idx);
	}
	/* reviewed */
	[CCode (cname = "filetype_id", cprefix = "GEANY_FILETYPES_", has_type_id = false)]
	public enum FiletypeID {
		NONE,
		
		PHP,
		BASIC,
		MATLAB,
		RUBY,
		LUA,
		FERITE,
		YAML,
		C,
		NSIS,
		GLSL,
		PO,
		MAKE,
		TCL,
		XML,
		CSS,
		REST,
		HASKELL,
		JAVA,
		CAML,
		AS,
		R,
		DIFF,
		HTML,
		PYTHON,
		CS,
		PERL,
		VALA,
		PASCAL,
		LATEX,
		ASM,
		CONF,
		HAXE,
		CPP,
		SH,
		FORTRAN,
		SQL,
		F77,
		DOCBOOK,
		D,
		JS,
		VHDL,
		ADA,
		CMAKE,
		MARKDOWN,
		TXT2TAGS,
		ABC,
		VERILOG,
		FORTH,
		LISP,
		ERLANG,
		COBOL,
		/* ^ append items here */
		[CCode (cname = "GEANY_MAX_BUILT_IN_FILETYPES")]
		MAX_BUILT_IN_FILETYPES
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_FILETYPE_GROUP_", has_type_id = false)]
	public enum FiletypeGroupID
	{
		NONE,
		COMPILED,
		SCRIPT,
		MARKUP,
		MISC,
		CUSTOM,
		COUNT
	}
	/* reviewed */
	[Compact]
	[CCode (cname = "struct GeanyFiletype", cprefix = "filetypes_")]
	public class Filetype {
		public string					context_action_cmd;
		public string?					comment_close;
		public string?					comment_open;
		public bool						comment_use_indent;
		public string?					error_regex_string;

		public string?					extension;
		public FiletypeGroupID			group;
		public Gdk.Pixbuf?				icon;
		public FiletypeID				id;
		public TagManager.LangType		lang;
		public Filetype					lexer_filetype;
		public string?					mime_type;
		public string					name;
		[CCode (array_length = false, array_null_terminated = true)]
		public string[]					pattern;
		public string					title;
		
		public unowned string			display_name {
			[CCode (cname = "filetypes_get_display_name")]
			get;
		}
		
		public static unowned Filetype	detect_from_file (string utf8_filename);
		public static unowned Filetype?	index (int idx);
		public static unowned Filetype?	lookup_by_name (string name);
	}
	/* reviewed */
	[CCode (cname = "filetypes", array_length_cexpr = "GEANY(filetypes_array)->len")]
	public unowned Filetype[] filetypes;
	/* reviewed */
	[CCode (cname = "filetypes_by_title")]
	public GLib.SList<Filetype> filetypes_by_title;
	/* reviewed */
	[Compact]
	public class Functions {
		/* No need to fill-in, the functions are wrapped.
		 * However, the plugins needs to define geany_functions with this type,
		 * so we need to have it here. */
	}
	/* reviewed */
	[CCode (cprefix = "highlighting_")]
	namespace Highlighting {
		public unowned LexerStyle	get_style (int ft_id, int style_id);
		public bool					is_code_style (int lexer, int style);
		public bool					is_comment_style (int lexer, int style);
		public bool					is_string_style (int lexer, int style);
		public void					set_styles (Sci sci, Filetype ft);
	}
	/* reviewed */
	public delegate void KeyCallback (uint key_id);
	/* reviewed */
	[CCode (has_type_id = false)]
	public struct KeyBinding {
		public KeyCallback		@callback;
		public uint				key;
		public string 			label;
		public Gtk.Widget?		menu_item;
		public Gdk.ModifierType	mods;
		public string 			name;
	}
	/* reviewed */
	[CCode (cprefix = "keybindings_")]
	namespace Keybindings {
		public unowned KeyBinding	get_item (KeyGroup group, size_t key_id);
		public void					send_command (KeyGroupID group_id, KeyBindingID key_id);
		public unowned KeyBinding	set_item (KeyGroup group, size_t key_id, KeyCallback? cb,
											  uint key, Gdk.ModifierType mod, string kf_name,
											  string label, Gtk.Widget? menu_item = null);
	}
	/* reviewed */
	[CCode (has_type_id = false)]
	public struct KeyGroup {}
	/* reviewed */
	public delegate bool KeyGroupCallback (uint key_id);
	/* reviewed */
	[CCode (cprefix = "GEANY_KEYS_", has_type_id = false)]
	public enum KeyBindingID {
		EDITOR_TRANSPOSELINE,
		DOCUMENT_REMOVE_ERROR_INDICATORS,
		FOCUS_SEARCHBAR,
		SEARCH_FIND,
		FILE_SAVEALL,
		GOTO_NEXTMARKER,
		NOTEBOOK_SWITCHTABLEFT,
		VIEW_ZOOMOUT,
		GOTO_LINE,
		DOCUMENT_TOGGLEFOLD,
		BUILD_COMPILE,
		EDITOR_SCROLLTOLINE,
		DOCUMENT_UNFOLDALL,
		GOTO_MATCHINGBRACE,
		SEARCH_FINDDOCUMENTUSAGE,
		CLIPBOARD_PASTE,
		BUILD_MAKE,
		INSERT_ALTWHITESPACE,
		EDITOR_SCROLLLINEDOWN,
		VIEW_TOGGLEALL,
		VIEW_FULLSCREEN,
		GOTO_LINEEND,
		EDITOR_CALLTIP,
		FILE_PRINT,
		EDITOR_DUPLICATELINE,
		FOCUS_SCRIBBLE,
		TOOLS_OPENCOLORCHOOSER,
		SEARCH_PREVIOUSMESSAGE,
		FILE_CLOSE,
		DOCUMENT_REPLACETABS,
		FILE_RELOAD,
		SEARCH_FINDNEXTSEL,
		FOCUS_MESSAGES,
		BUILD_RUN,
		HELP_HELP,
		SETTINGS_PLUGINPREFERENCES,
		VIEW_ZOOMRESET,
		SELECT_WORD,
		FORMAT_INCREASEINDENT,
		SETTINGS_PREFERENCES,
		FORMAT_SENDTOCMD3,
		DOCUMENT_FOLDALL,
		FORMAT_SENDTOVTE,
		PROJECT_PROPERTIES,
		DOCUMENT_LINEWRAP,
		EDITOR_MACROLIST,
		EDITOR_SUPPRESSSNIPPETCOMPLETION,
		FOCUS_SIDEBAR_SYMBOL_LIST,
		GOTO_LINESTART,
		SEARCH_FINDUSAGE,
		FILE_NEW,
		EDITOR_SNIPPETNEXTCURSOR,
		NOTEBOOK_SWITCHTABRIGHT,
		FILE_SAVE,
		FORMAT_INCREASEINDENTBYSPACE,
		SEARCH_FINDNEXT,
		GOTO_TOGGLEMARKER,
		GOTO_TAGDEFINITION,
		SEARCH_NEXTMESSAGE,
		EDITOR_DELETELINETOEND,
		FORMAT_AUTOINDENT,
		FILE_OPENSELECTED,
		GOTO_BACK,
		INSERT_DATE,
		BUILD_PREVIOUSERROR,
		GOTO_LINEENDVISUAL,
		DOCUMENT_REPLACESPACES,
		FOCUS_EDITOR,
		SELECT_WORDPARTRIGHT,
		VIEW_MESSAGEWINDOW,
		FOCUS_SIDEBAR_DOCUMENT_LIST,
		FORMAT_REFLOWPARAGRAPH,
		EDITOR_MOVELINEUP,
		NOTEBOOK_MOVETABLEFT,
		SELECT_LINE,
		EDITOR_UNDO,
		EDITOR_MOVELINEDOWN,
		CLIPBOARD_COPYLINE,
		BUILD_MAKEOWNTARGET,
		FORMAT_SENDTOCMD2,
		SEARCH_MARKALL,
		BUILD_LINK,
		FILE_CLOSEALL,
		GOTO_FORWARD,
		CLIPBOARD_CUT,
		NOTEBOOK_SWITCHTABLASTUSED,
		NOTEBOOK_MOVETABRIGHT,
		BUILD_OPTIONS,
		GOTO_TAGDECLARATION,
		FILE_OPEN,
		EDITOR_COMPLETESNIPPET,
		FORMAT_UNCOMMENTLINE,
		FOCUS_VTE,
		FORMAT_SENDTOCMD1,
		SELECT_WORDPARTLEFT,
		VIEW_ZOOMIN,
		DOCUMENT_LINEBREAK,
		EDITOR_REDO,
		EDITOR_CONTEXTACTION,
		SEARCH_FINDPREVSEL,
		FORMAT_DECREASEINDENTBYSPACE,
		FORMAT_COMMENTLINETOGGLE,
		SELECT_ALL,
		DOCUMENT_RELOADTAGLIST,
		BUILD_NEXTERROR,
		NOTEBOOK_MOVETABLAST,
		SELECT_PARAGRAPH,
		EDITOR_DELETELINE,
		CLIPBOARD_COPY,
		VIEW_SIDEBAR,
		FILE_SAVEAS,
		FORMAT_COMMENTLINE,
		GOTO_PREVWORDPART,
		SEARCH_FINDPREVIOUS,
		SEARCH_REPLACE,
		EDITOR_WORDPARTCOMPLETION,
		EDITOR_AUTOCOMPLETE,
		FOCUS_SIDEBAR,
		FOCUS_MESSAGE_WINDOW,
		NOTEBOOK_MOVETABFIRST,
		GOTO_PREVIOUSMARKER,
		EDITOR_SCROLLLINEUP,
		FOCUS_COMPILER,
		FORMAT_TOGGLECASE,
		CLIPBOARD_CUTLINE,
		DOCUMENT_REMOVE_MARKERS,
		BUILD_MAKEOBJECT,
		FORMAT_DECREASEINDENT,
		FILE_OPENLASTTAB,
		SEARCH_FINDINFILES,
		GOTO_NEXTWORDPART,
		INSERT_LINEAFTER,
		INSERT_LINEBEFORE
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_KEY_GROUP_", has_type_id = false)]
	public enum KeyGroupID {
		FILE,
		PROJECT,
		EDITOR,
		CLIPBOARD,
		SELECT,
		FORMAT,
		INSERT,
		SETTINGS,
		SEARCH,
		GOTO,
		VIEW,
		FOCUS,
		NOTEBOOK,
		DOCUMENT,
		BUILD,
		TOOLS,
		HELP
	}
	/* reviewed */
	[Compact]
	public class LexerStyle {
		public int	foreground;
		public int	background;
		public bool	bold;
		public bool	italic;
	}
	/* reviewed */
	[CCode (cprefix = "main_")]
	namespace Main {
		public void		reload_configuration ();
		public void		locale_init (string locale_dir, string package);
		public bool		is_realized ();
	}
	/* reviewed */
	[Compact]
	public class MainWidgets {
		/* Main */
		/** Gets the main window. */
		[CCode (cname="window")] Gtk.Widget _window;
		public Gtk.Window window {
			[CCode (cname="geany_vala_plugin_main_widgets_get_main_window")]
			get { return _window as Gtk.Window; }
		}
		
		/** Gets the main toolbar. */
		[CCode (cname="toolbar")] Gtk.Widget _toolbar;
		public Gtk.Toolbar toolbar {
			[CCode (cname="geany_vala_plugin_main_widgets_get_toolbar")]
			get { return _toolbar as Gtk.Toolbar; }
		}
		
		/**
		 * Gets the progress bar widget in the statusbar.
		 * 
		 * This progress bar can be used to show progress of various
		 * actions.
		 */
		[CCode (cname="progressbar")] Gtk.Widget _progressbar;
		public Gtk.ProgressBar progressbar {
			[CCode (cname="geany_vala_plugin_main_widgets_get_progressbar")]
			get { return _progressbar as Gtk.ProgressBar; }
		}
		
		/* Menus */
		/** Gets the popup editor menu. */
		[CCode (cname="editor_menu")] Gtk.Widget _editor_menu;
		public Gtk.MenuShell editor_menu {
			[CCode (cname="geany_vala_plugin_main_widgets_get_editor_menu")]
			get { return _editor_menu as Gtk.MenuShell; }
		}
		
		/** 
		 * Gets the Project menu.
		 * 
		 * Plugins modifying the project can add their items to the
		 * Project menu.
		 */
		[CCode (cname="project_menu")] Gtk.Widget _project_menu;
		public Gtk.MenuShell project_menu {
			[CCode (cname="geany_vala_plugin_main_widgets_get_project_menu")]
			get { return _project_menu as Gtk.MenuShell; }
		}
		
		/**
		 * Gets the Tools menu.
		 * 
		 * Most plugins add menu items to the Tools menu.
		 */
		[CCode (cname="tools_menu")] Gtk.Widget _tools_menu;
		public Gtk.MenuShell tools_menu {
			[CCode (cname="geany_vala_plugin_main_widgets_get_tools_menu")]
			get { return _tools_menu as Gtk.MenuShell; }
		}
		
		/* Notebooks */
		/** Gets the notebook containing the documents. */
		[CCode (cname="notebook")] Gtk.Widget _documents_notebook;
		public Gtk.Notebook documents_notebook {
			get { return _documents_notebook as Gtk.Notebook; }
		}
		
		/** Gets the bottom message window notebook. */
		[CCode (cname="message_window_notebook")] Gtk.Widget _message_window_notebook;
		public Gtk.Notebook message_window_notebook {
			[CCode (cname="geany_vala_plugin_main_widgets_get_message_window_notebook")]
			get { return _message_window_notebook as Gtk.Notebook; }
		}
		
		/** Gets the sidebar notebook. */
		[CCode (cname="sidebar_notebook")] Gtk.Widget _sidebar_notebook;
		public Gtk.Notebook sidebar_notebook {
			[CCode (cname="geany_vala_plugin_main_widgets_get_sidebar_notebook")]
			get { return _sidebar_notebook as Gtk.Notebook; }
		}
	}
	/* reviewed */
	[CCode (lower_case_cprefix = "msgwin_", cprefix = "msgwin_")]
	namespace MessageWindow {
		[CCode (cname = "MessageWindowTabNum", has_type_id = false)]
		public enum TabID {
			STATUS,
			COMPILER,
			MESSAGE,
			SCRATCH,
			VTE
		}
		[CCode (cname = "MsgColors", has_type_id = false)]
		public enum Color {
			RED,
			DARK_RED,
			BLACK,
			BLUE
		}

		public void		clear_tab (MessageWindow.TabID tabnum);
		[PrintfFormat]
		public void		compiler_add (MessageWindow.Color msg_color, string format, ...);
		[PrintfFormat]
		public void		msg_add (MessageWindow.Color msg_color, int line, Document? doc, string format, ...);
		[PrintfFormat]
		public void		set_messages_dir (string messages_dir);
		public void		status_add (string format, ...);
		public void		switch_tab (MessageWindow.TabID tabnum, bool show);
	}
	/* reviewed */
	[CCode (cprefix = "navqueue_")]
	namespace NavQueue {
		public bool		goto_line (Document? old_doc, Document new_doc, int line);
	}
	/* reviewed */
	[Compact]
	[CCode (cprefix = "plugin_")]
	public class Plugin {
		[Compact]
		[CCode (cname = "PluginInfo")]
		public class Info {
			public unowned string name;
			public unowned string? description;
			public unowned string? version;
			public unowned string? author;
			
			[CCode (cname = "geany_vala_plugin_SET_INFO")]
			public void @set (string name, string? description, string? version, string? author)
			{
				this.name			= name;
				this.description	= description;
				this.version		= version;
				this.author			= author;
			}
		}
		[Compact]
		[CCode (cname = "PluginCallback")]
		public class Callback {
			public unowned string	signal_name;
			public GLib.Callback	@callback;
			public bool				after;
			public void				*user_data;
		}
		[Compact]
		[Deprecated (replacement = "Ui.add_document_sensitive()")]
		[CCode (cname = "PluginFields")]
		public class Fields {
			public Flags		flags;
			public Gtk.MenuItem	menu_item;
		}
		[Deprecated (replacement = "Ui.add_document_sensitive()")]
		[CCode (cname = "PluginFlags", has_type_id = false)]
		public enum Flags {
			[CCode (cname = "GEANY_IS_DOCUMENT_SENSITIVE")]
			IS_DOCUMENT_SENSITIVE
		}
		
		[CCode (cname = "geany_vala_plugin_VERSION_CHECK")]
		public static int version_check (int abi_version, int api_required) {
			/* drop-in copy of GEANY_VERSION_CHECK() macro */
			if (abi_version != ABI_VERSION)
				return -1;
			return (api_required);
		}
		
		[CCode (cname = "GEANY_API_VERSION")]
		static int API_VERSION;
		[CCode (cname = "GEANY_ABI_VERSION")]
		static int ABI_VERSION;
		
		public Info info;
		
		public void		add_toolbar_item (Gtk.ToolItem item);
		public void		module_make_resident ();
		public void		signal_connect (GLib.Object? object, string signal_name, bool after,
										GLib.Callback cb, void *user_data = null);
		public KeyGroup	set_key_group (string section_name, size_t count, KeyGroupCallback cb);
		public void		show_configure ();
	}
	/* reviewed */
	[Compact]
	public class Project {
		public string	base_path;
		public string	description;
		public string	file_name;
		[CCode (array_length = false, array_null_terminated = true)]
		public string[]	file_patterns;
		public string	name;
		public int		type;
	}
	/* TODO: switch to codebrainz's full wrapper */
	[CCode (cname = "ScintillaObject", cprefix = "scintilla_")]
	public class Scintilla : Gtk.Container {
		[CCode (cprefix = "SCI_")]
		public enum Message {
			GETEOLMODE,
			LINEDELETE
		}
		[CCode (cprefix = "SC_EOL_")]
		public enum EolMode {
			CRLF,
			CR,
			LF
		}
		
		public Scintilla ();
		public long		send_message (uint iMessage, ulong wParam = 0, ulong lParam = 0);
	}
	/* reviewed */
	/* TODO: use proper types for the argument when we get them (enums & co) */
	[CCode (cname = "ScintillaObject", cprefix = "sci_")]
	public class Sci : Scintilla {
		/**
		 * Creates a new {@link Sci} widget based on the settings in //editor//.
		 * 
		 * @param editor The editor who's settings to use the create the 
		 *               {@link Sci} widget.
		 * 
		 * @return A new {@link Sci} widget.
		 */
		[CCode (cname = "editor_create_widget")]
		public Sci (Editor editor) {}
		
		/* FIXME: we need this to be really implemented for find_text() to be usable */
		public struct TextToFind {
		}
		
		public void				delete_marker_at_line (int line_number, int marker);
		public void				end_undo_action ();
		public void				ensure_line_is_visible (int line);
		public int				find_matching_brace (int pos);
		public int				find_text (int flags, TextToFind ttf);
		public char				get_char_at (int pos);
		public int				get_col_from_position (int position);
		public string			get_contents (int len = this.get_length () + 1);
		public string			get_contents_range (int start, int end);
		public int				get_current_line ();
		public int				get_current_position ();
		public int				get_length ();
		/* make get_line() take an optional second parameter to choose whether
		 * to include the EOL character */
		[CCode (cname = "sci_get_line")]
		private string			__get_line_with_eol (int line_num);
		[CCode (cname = "geany_vala_plugin_sci_get_line")]
		public string			get_line (int line_num, bool include_eol = true) {
			if (include_eol) {
				return this.__get_line_with_eol (line_num);
			} else {
				return this.get_contents_range (this.get_position_from_line (line_num),
												this.get_line_end_position (line_num));
			}
		}
		public int				get_line_count ();
		public int				get_line_end_position (int line);
		public int				get_line_from_position (int position);
		public int				get_line_indentation (int line);
		public bool				get_line_is_visible (int line);
		public int				get_line_length (int line);
		public int				get_position_from_line (int line);
		[Deprecated (replacement = "Geany.Sci.get_selection_contents")]
		public void				get_selected_text (string text);
		public int				get_selected_text_length ();
		public string			get_selection_contents ();
		public int				get_selection_end ();
		public int				get_selection_mode ();
		public int				get_selection_start ();
		public int				get_style_at (int position);
		public int				get_tab_width ();
		public void				goto_line (int line, bool unfold = true);
		public bool				has_selection ();
		public void				indicator_clear (int start, int end);
		public void				indicator_set (int indic);
		public void 			insert_text (int pos, string text);
		public bool				is_marker_set_at_line (int line, int marker);
		[CCode (cname = "sci_replace_sel")]
		public void				replace_selection (string text);
		public int				replace_target (string text, bool regex = false);
		public void				scroll_caret ();
		/* FIXME: maybe remove this since we have default values for send_message()? */
		[Deprecated (replacement = "Scintilla.send_message")]
		public void				send_command (int cmd);
		public void				set_current_position (int position, bool scroll_to_caret = true);
		public void				set_font (int style, string font, int size);
		public void				set_line_indentation (int line, int indent);
		public void				set_marker_at_line (int line_number, int marker);
		public void				set_selection_end (int position);
		public void				set_selection_mode (int mode);
		public void				set_selection_start (int position);
		public void				set_target_end (int end);
		public void				set_target_start (int start);
		public void				set_text (string text);
		public void				start_undo_action ();

		/* these ones aren't exported in the plugin API or doesn't exist at all */
		[CCode (cname = "geany_vala_plugin_sci_get_eol_mode")]
		public int get_eol_mode () {
			return (int)this.send_message (Scintilla.Message.GETEOLMODE);
		}
		[CCode (cname = "geany_vala_plugin_sci_get_eol_char_len")]
		public int get_eol_char_len () {
			switch (this.get_eol_mode ()) {
				case EolMode.CRLF:	return 2;
				default:			return 1;
			}
		}
	}
	/* reviewed */
	[CCode (cprefix = "search_")]
	namespace Search {
		public void		show_find_in_files_dialog (string? dir = null);
	}
	/* reviewed */
	[Compact]
	[CCode (cname = "StashGroup",
			cprefix = "stash_group_",
			free_function = "stash_group_free")]
	public class StashGroup {
		
		public StashGroup (string name);
		
		public void		add_boolean (ref bool setting, string key_name, bool default_value);
		public void		add_combo_box (ref int setting, string key_name, int default_value, void *widget_id);
		public void		add_combo_box_entry (ref string setting, string key_name, string default_value,
											 void *widget_id);
		public void		add_entry (ref string setting, string key_name, string default_value,
								   void *widget_id);
		public void		add_integer (ref int setting, string key_name, int default_value);
		public void		add_radio_buttons (ref int setting, string key_name, int default_value,
										   void *widget_id, int enum_id, ...);
		public void		add_spin_button_integer (ref int setting, string key_name, int default_value,
												 void *widget_id);
		public void		add_string (ref string setting, string key_name, string? default_value);
		public void		add_string_vector (ref string[] setting, string key_name, string[]? default_value);
		public void		add_toggle_button (ref bool setting, string key_name, bool default_value,
										   void *widget_id);
		public void		add_widget_property (void *setting, string key_name, void *default_value,
											 void *widget_id, string property_name, GLib.Type type);
		public void		display (Gtk.Widget? owner);
		public bool		load_from_file (string filename);
		public void		load_from_key_file (GLib.KeyFile keyfile);
		public int		save_to_file (string filename, GLib.KeyFileFlags flags = GLib.KeyFileFlags.NONE);
		public void		save_to_key_file (GLib.KeyFile keyfile);
		public void		update (Gtk.Widget? owner);
	}
	/* reviewed */
	[CCode (cprefix = "symbols_")]
	namespace Symbols {
		public unowned string	get_context_separator (FiletypeID ft_id);
	}
	/* reviewed */
	[CCode (cprefix = "templates_")]
	namespace Templates {
		public string	get_template_fileheader (int filetype_idx, string fname);
	}
	/* reviewed */
	[CCode (cprefix = "TM", lower_case_cprefix = "tm_")]
	namespace TagManager {
		/* TODO: add TMFileEntry? not sure it's useful */
		[Compact]
		[CCode (free_function = "tm_work_object_free")]
		public class WorkObject {
			public uint				type;
			public string			file_name;
			public string			short_name;
			public WorkObject?		parent;
			public time_t			analyze_time;
			public GLib.PtrArray	tags_array;
		}
		[Compact]
		public class Workspace : WorkObject {
			public GLib.PtrArray	global_tags;
			public GLib.PtrArray	work_objects;
			
			public static bool		add_object (WorkObject work_object);
			public static bool		remove_object (WorkObject w, bool do_free, bool update);
		}
		[Compact]
		public class SourceFile : WorkObject {
			/**
			 * Programming language used
			 */
			public LangType		lang;
			/**
			 * Whether this file should be scanned for tags
			 */
			public bool			inactive;
			
			/* Methods */
			
			public SourceFile (string? file_name, bool update, string? name = null);
			public bool			update (bool force = true, bool recurse = true, bool update_parent = true);
		}
		[Compact]
		public class Project : WorkObject {
			/**
			 * Top project directory
			 */
			public string			dir;
			/**
			 * Extensions for source files (wildcards, NULL terminated)
			 */
			[CCode (array_length = false, array_null_terminated = true)]
			public string[]			sources;
			/**
			 * File patters to ignore
			 */
			[CCode (array_length = false, array_null_terminated = true)]
			public string[]			ignore;
			/**
			 * Array of {@link TagManager.SourceFile}s present in the project
			 */
			public GLib.PtrArray	file_list;
		}
		[SimpleType]
		[IntegerType]
		[CCode (cname = "langType")]
		public struct LangType : int {
		}
		[Flags]
		[CCode (has_type_id = false)]
		public enum TagType {
			[CCode (cname = "tm_tag_undef_t")]
			UNDEF,
			[CCode (cname = "tm_tag_class_t")]
			CLASS,
			[CCode (cname = "tm_tag_enum_t")]
			ENUM,
			[CCode (cname = "tm_tag_enumerator_t")]
			ENUMERATOR,
			[CCode (cname = "tm_tag_field_t")]
			FIELD,
			[CCode (cname = "tm_tag_function_t")]
			FUNCTION,
			[CCode (cname = "tm_tag_interface_t")]
			INTERFACE,
			[CCode (cname = "tm_tag_member_t")]
			MEMBER,
			[CCode (cname = "tm_tag_method_t")]
			METHOD,
			[CCode (cname = "tm_tag_namespace_t")]
			NAMESPACE,
			[CCode (cname = "tm_tag_package_t")]
			PACKAGE,
			[CCode (cname = "tm_tag_prototype_t")]
			PROTOTYPE,
			[CCode (cname = "tm_tag_struct_t")]
			STRUCT,
			[CCode (cname = "tm_tag_typedef_t")]
			TYPEDEF,
			[CCode (cname = "tm_tag_union_t")]
			UNION,
			[CCode (cname = "tm_tag_variable_t")]
			VARIABLE,
			[CCode (cname = "tm_tag_externvar_t")]
			EXTERNVAR,
			[CCode (cname = "tm_tag_macro_t")]
			MACRO,
			[CCode (cname = "tm_tag_macro_with_arg_t")]
			MACRO_WITH_ARG,
			[CCode (cname = "tm_tag_file_t")]
			FILE,
			[CCode (cname = "tm_tag_other_t")]
			OTHER,
			[CCode (cname = "tm_tag_max_t")]
			MAX
		}
		[Flags]
		[CCode (has_type_id = false)]
		public enum TagAttrType {
			[CCode (cname = "tm_tag_attr_none_t")]
			NONE,
			[CCode (cname = "tm_tag_attr_name_t")]
			NAME,
			[CCode (cname = "tm_tag_attr_type_t")]
			TYPE,
			[CCode (cname = "tm_tag_attr_file_t")]
			FILE,
			[CCode (cname = "tm_tag_attr_line_t")]
			LINE,
			[CCode (cname = "tm_tag_attr_pos_t")]
			POS,
			[CCode (cname = "tm_tag_attr_scope_t")]
			SCOPE,
			[CCode (cname = "tm_tag_attr_inheritance_t")]
			INHERITANCE,
			[CCode (cname = "tm_tag_attr_arglist_t")]
			ARGLIST,
			[CCode (cname = "tm_tag_attr_local_t")]
			LOCAL,
			[CCode (cname = "tm_tag_attr_time_t")]
			TIME,
			[CCode (cname = "tm_tag_attr_vartype_t")]
			VARTYPE,
			[CCode (cname = "tm_tag_attr_access_t")]
			ACCESS,
			[CCode (cname = "tm_tag_attr_impl_t")]
			IMPL,
			[CCode (cname = "tm_tag_attr_lang_t")]
			LANG,
			[CCode (cname = "tm_tag_attr_inactive_t")]
			INACTIVE,
			[CCode (cname = "tm_tag_attr_pointer_t")]
			POINTER,
			[CCode (cname = "tm_tag_attr_max_t")]
			MAX
		}
		[CCode (cprefix = "TAG_ACCESS_")]
		namespace TagAccess {
			char PUBLIC; /*!< Public member */
			char PROTECTED; /*!< Protected member */
			char PRIVATE; /*!< Private member */
			char FRIEND; /*!< Friend members/functions */
			char DEFAULT; /*!< Default access (Java) */
			char UNKNOWN; /*!< Unknown access type */
		}
		[CCode (cprefix = "TAG_IMPL_")]
		namespace TagImplementation {
			char VIRTUAL; /*!< Virtual implementation */
			char UNKNOWN; /*!< Unknown implementation */
		}
		/* this is a dummy proxy structure because Vala doesn't support inline anonymous structs */
		[CCode (cname = "__GeanyValaPluginTMTagAttributesEntry")]
		public struct TagAttributesEntry {
			public SourceFile	file;
			public ulong		line;
			public bool			local;
			[CCode (cname = "pointerOrder")]
			public uint			pointer_order;
			public string?		arglist;
			public string?		scope;
			public string?		inheritance;
			public string?		var_type;
			public char			access;
			public char			impl;
		}
		/* this is a dummy proxy structure because Vala doesn't support inline anonymous structs */
		[CCode (cname = "__GeanyValaPluginTMTagAttributesFile")]
		public struct TagAttributesFile {
			public time_t		timestamp;
			public LangType		lang;
			public bool			inactive;
		}
		/* this is a dummy proxy structure because Vala doesn't support inline anonymous structs */
		[CCode (cname = "__GeanyValaPluginTMTagAttributes")]
		public struct TagAttributes {
			public TagAttributesEntry	entry;
			public TagAttributesFile	file;
		}
		[Compact]
		[CCode (cname = "TMTag", cprefix = "tm_tag_")]
		public class Tag {
			public string			name;
			public TagType			type;
			[CCode (cname = "atts")]
			public TagAttributes	attributes;
		}
		/* this is a dummy proxy structure because Vala doesn't support inline anonymous structs */
		[CCode (cname = "__GeanyValaPluginTMSymbolInfo")]
		public struct SymbolInfo {
			/**
			 * Array of child symbols
			 */
			public GLib.PtrArray	children;
			/**
			 * Prototype tag for functions
			 */
			public Tag				equiv;
		}
		[Compact]
		[CCode (cname = "TMSymbol", cprefix = "tm_symbol_tree_" /*, free_function = "tm_symbol_tree_free"*/)]
		public class Symbol {
			/**
			 * The tag corresponding to this symbol
			 */
			public Tag			tag;
			/**
			 * Parent class/struct for functions/variables 
			 */
			public Symbol?		parent;
			public SymbolInfo	info;
		}
		
		public string		get_real_path (string file_name);
	}
	/* reviewed */
	[CCode (cprefix = "ui_", lower_case_cprefix = "ui_")]
	namespace Ui {
		[CCode (cname = "GtkButton", type_id = "GTK_TYPE_BUTTON")]
		public class Button : Gtk.Button {
			public Button.with_image (string stock_id, string text);
		}
		[CCode (cname = "GtkImageMenuItem", type_id = "GTK_TYPE_IMAGE_MENU_ITEM")]
		public class ImageMenuItem : Gtk.ImageMenuItem {
			public ImageMenuItem (string stock_id, string label);
		}
		[CCode (cname = "GtkHBox", type_id = "GTK_TYPE_HBOX")]
		public class PathBox : Gtk.HBox {
			public PathBox (string? title, Gtk.FileChooserAction action, Gtk.Entry entry);
		}
		[CCode (cname = "GtkVBox", type_id = "GTK_TYPE_VBOX")]
		public class DialogVBox : Gtk.VBox {
			public DialogVBox (Gtk.Dialog dialog);
		}
		[CCode (cname = "GtkFrame", type_id = "GTK_TYPE_FRAME")]
		public class Frame : Gtk.Frame {
			public Frame.with_alignment (string label_text, out Gtk.Alignment alignment);
		}
		
		public void					hookup_widget (Gtk.Widget owner, Gtk.Widget widget, string name);
		public void					add_document_sensitive (Gtk.Widget widget);
		/*public Gtk.Button			button_new_with_image (string stock_id, string text);*/
		public void					combo_box_add_to_history (Gtk.ComboBoxEntry combo_entry, string? text,
															  int history_len = 0);
		/*public Gtk.VBox			dialog_vbox_new (Gtk.Dialog dialog);*/
		public void					entry_add_clear_icon (Gtk.Entry entry);
		/*public Gtk.Frame			frame_new_with_alignment (string label_text, out Gtk.Alignment alignment);*/
		public int					get_gtk_settings_integer (string property_name, int default_value);
		/*public Gtk.ImageMenuItem	image_menu_item_new (string stock_id, string label);*/
		public bool					is_keyval_enter_or_return (uint keyval);
		public unowned Gtk.Widget?	lookup_widget (Gtk.Widget widget, string widget_name);
		public void					menu_add_document_items (Gtk.Menu menu, Document? active, GLib.Callback cb);
		/*public Gtk.HBox			path_box_new (string? title, Gtk.FileChooserAction action, Gtk.Entry entry);*/
		public void					progress_bar_start (string? text);
		public void					progress_bar_stop ();
		[PrintfFormat]
		public void					set_statusbar (bool log, string format, ...);
		public void					table_add_row (Gtk.Table table, int row, ...);
		public void					widget_modify_font_from_string (Gtk.Widget widget, string str);
		public void					widget_set_tooltip_text (Gtk.Widget widget, string text);
	}
	/* reviewed */
	[CCode (cprefix = "utils_", lower_case_cprefix = "utils_")]
	namespace Utils {
		[CCode (array_length = false, array_null_terminated = true)]
		public string[]				copy_environment ([CCode (array_length = false, array_null_terminated = true)]
													  string[]? exclude_vars, string first_varname, ...);
		public string?				find_open_xml_tag (string sel, int size);
		public string				get_date_time (string format, time_t? time_to_use = null);
		public GLib.SList<string>?	get_file_list (string path, out uint length = null) throws GLib.Error;
		public GLib.SList<string>?	get_file_list_full (string path, bool full_path = false, bool sort = false)
			throws GLib.Error;
		public string				get_locale_from_utf8 (string utf8_text);
		public bool					get_setting_boolean (GLib.KeyFile config, string section,
														 string key, bool default_value);
		public int					get_setting_integer (GLib.KeyFile config, string section,
														 string key, int default_value);
		public string				get_setting_string (GLib.KeyFile config, string section,
														string key, string default_value);
		public string				get_utf8_from_locale (string locale_text);
		public int					mkdir (string path, bool create_parent_dirs = true);
		public void					open_browser (string uri);
		public string				remove_ext_from_filename (string filename);
		public bool					spawn_async (string? dir,
												 [CCode (array_length = false, array_null_terminated = true)] string[] argv,
												 [CCode (array_length = false, array_null_terminated = true)] string[]? env = null,
												 GLib.SpawnFlags flags = 0, GLib.SpawnChildSetupFunc? child_setup = null,
												 out GLib.Pid child_pid = null) throws GLib.Error;
		public bool					spawn_sync (string? dir,
												[CCode (array_length = false, array_null_terminated = true)] string[] argv,
												[CCode (array_length = false, array_null_terminated = true)] string[]? env = null,
												GLib.SpawnFlags flags = 0, GLib.SpawnChildSetupFunc? child_setup = null,
												out string std_out = null, out string std_err = null,
												out int exit_status = null) throws GLib.Error;
		public int					str_casecmp (string? s1, string? s2);
		/* utils_str_equal() -> no need, Vala has "==" for this */
		public string				str_middle_truncate (string str, uint truncate_length);
		/* wrap str_remove_chars() to return a copy of the string to fit Vala's
		 * conventions, and because I can't find the right binding for all cases */
		[CCode (cname = "utils_str_remove_chars")]
		private unowned string		__str_remove_chars (string str, string chars);
		[CCode (cname = "geany_vala_plugin_utils_str_remove_chars")]
		public string				str_remove_chars (string str, string chars) {
			var copy = str;
			__str_remove_chars (copy, chars);
			return copy;
		}
		public uint					string_replace_all (GLib.StringBuilder haystack, string needle, string replace);
		public uint					string_replace_first (GLib.StringBuilder haystack, string needle, string replace);
		public int					write_file (string filename, string text);
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_INDENT_TYPE_", has_type_id = false)]
	public enum IndentType {
		SPACES,
		TABS,
		BOTH
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_AUTOINDENT_", has_type_id = false)]
	public enum AutoIndent {
		NONE,
		BASIC,
		CURRENTCHARS,
		MATCHBRACES
	}
	/* reviewed */
	[CCode (cprefix = "GEANY_VIRTUAL_SPACE_", has_type_id = false)]
	public enum VirtualSpace {
		DISABLED,
		SELECTION,
		ALWAYS
	}
}
