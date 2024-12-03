configure_file(
	input: 'geany.1.in',
	output: 'geany.1',
	install: true,
	install_dir: join_paths(join_paths(prefix, get_option('mandir'), 'man1')),
	configuration: cdata
)

if rst2pdf.found()
	custom_target('pdf-manual',
		input: ['geany.txt'],
		output: ['geany-@0@.pdf'.format(cdata.get('VERSION'))],
		command: [rst2pdf, '@INPUT@', '-o', '@OUTPUT@'],
		build_by_default: true
	)
endif

tarball = run_command('test', '-f', 'geany.html', check: false)
if tarball.returncode() == 0 or rst2html.found()
	install_data(
		'images/build_menu_commands_dialog.png',
		'images/edit_change_history.png',
		'images/find_dialog.png',
		'images/find_in_files_dialog.png',
		'images/main_window.png',
		'images/pref_dialog_edit_completions.png',
		'images/pref_dialog_edit_display.png',
		'images/pref_dialog_edit_features.png',
		'images/pref_dialog_edit_indentation.png',
		'images/pref_dialog_files.png',
		'images/pref_dialog_gen_misc.png',
		'images/pref_dialog_gen_startup.png',
		'images/pref_dialog_interface_interface.png',
		'images/pref_dialog_interface_notebook.png',
		'images/pref_dialog_interface_toolbar.png',
		'images/pref_dialog_keys.png',
		'images/pref_dialog_printing.png',
		'images/pref_dialog_templ.png',
		'images/pref_dialog_tools.png',
		'images/pref_dialog_various.png',
		'images/pref_dialog_vte.png',
		'images/replace_dialog.png',
		'images/sidebar_documents_only.png',
		'images/sidebar_show_paths.png',
		'images/sidebar_show_tree.png',
		install_dir: join_paths(cdata.get('GEANY_DOC_DIR'), 'html', 'images')
	)
	if tarball.returncode() == 0
		message('Building from tarball, installing prebuild html')
		geany_html = files('geany.html')
	else
		cmd = [rst2html, '-stg', '--stylesheet=@INPUT0@', '@INPUT1@', '@OUTPUT@']
		geany_html = custom_target('geany.html',
			input: ['geany.css', 'geany.txt'],
			output: ['geany.html'],
			build_by_default: true,
			command: cmd
		)
		custom_target('hacking.html',
			input: ['geany.css', '../HACKING'],
			output: ['hacking.html'],
			build_by_default: true,
			command: cmd
		)
	endif
	# geany.html is installed as index.html
	custom_target('index.html',
		input: [geany_html],
		output: ['index.html'],
		command: [ln, '-f', '@INPUT@', '@OUTPUT@'],
		install: true,
		install_dir: join_paths(cdata.get('GEANY_DOC_DIR'), 'html')
	)
endif

install_data('geany.txt',
	rename: 'manual.txt',
	install_dir: cdata.get('GEANY_DOC_DIR')
)

# Normally, doxygen is required for the gtkdoc headers
# but it can be disabled if really needed (e.g. if plugins are also disabled),
# packagers must not disable this!
if doxygen.found()
	doxcfg = configuration_data()
	doxcfg.merge_from(cdata)
	doxcfg.set('GIRONLY', '@internal')
	doxcfg.set('HTML', 'YES')
	doxcfg.set('XML', 'NO')
	doxcfg.set('SORT', 'YES')

	doxgicfg = doxcfg
	doxgicfg.set('GIRONLY', '')
	doxgicfg.set('HTML', 'NO')
	doxgicfg.set('XML', 'YES')
	doxgicfg.set('SORT', 'NO')

	dep_doxygen = files([
		'plugins.dox',
		'pluginsignals.c',
		'pluginsymbols.c',
		'stash-example.c',
		'stash-gui-example.c'
	])
	if python.found()
		doxyfile_gi = configure_file(
			input: 'Doxyfile.in',
			output: 'Doxyfile-gi',
			configuration: doxgicfg
		)
		doxygen_gi = custom_target('doxygen-gi',
			input: doxyfile_gi,
			# stamp file due https://github.com/mesonbuild/meson/issues/2320
			output: 'doxygen-gi.stamp',
			command: [sh, '-c', 'doxygen "$1" && touch "$2"', 'sh', '@INPUT@', '@OUTPUT@'],
			depends: libgeany,
			depend_files: dep_doxygen
		)
		gtkdoc_py = find_program('../scripts/gen-api-gtkdoc.py')
		custom_target('gtkdoc-headers',
			output: ['geany-gtkdoc.h', 'geany-sciwrappers-gtkdoc.h'],
			command: [gtkdoc_py, join_paths(meson.current_build_dir(), 'xml'), '-d', '.', '-o', '@OUTPUT0@', '--sci-output', '@OUTPUT1@'],
			depends: doxygen_gi,
			install: true,
			install_dir: join_paths(get_option('includedir'), 'geany', 'gtkdoc')
		)
	endif
	doxyfile = configure_file(
		input: 'Doxyfile.in',
		output: 'Doxyfile',
		configuration: doxcfg
	)
	custom_target('doxygen',
		input: doxyfile,
		# stamp file due https://github.com/mesonbuild/meson/issues/2320
		output: 'doxygen.stamp',
		command: [sh, '-c', 'doxygen "$1" && touch "$2"', 'sh', '@INPUT@', '@OUTPUT@'],
		depends: libgeany,
		depend_files: dep_doxygen,
		build_by_default: true
	)
endif
