plugin_inc = [include_directories('..', '../src', '../src/tagmanager')]
plugin_inc += iscintilla

plugins = [
	'Demoplugin',
	'Demoproxy',
	'Classbuilder',
	'HTMLChars',
	'Export',
	'SaveActions',
	'FileBrowser',
	'SplitWindow'
]

plugin_suffix = []
if (host_machine.system() == 'darwin')
	plugin_suffix = 'so'  # use "so" instead of "dylib" so Geany finds plugins correctly
endif

foreach plugin : plugins
	id = plugin.to_lower()
	skip_install = id.startswith('demo')
	shared_module(id,
		id + '.c',
		name_prefix: '', # "lib" seems to be the default prefix
		name_suffix: plugin_suffix,
		link_with: libgeany,
		include_directories: plugin_inc,
		c_args: [def_cflags, '-DG_LOG_DOMAIN="'+plugin+'"'],
		dependencies: [deps, dep_libgeany],
		install_dir: join_paths(prefix, get_option('libdir'), 'geany'),
		install: not skip_install
	)
endforeach
