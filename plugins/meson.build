plugin_inc = [include_directories('..', '../src', '../src/tagmanager')]
plugin_inc += iscintilla

plugins = ['Demoplugin']
plugins += 'Demoproxy'
plugins += 'HTMLChars'
plugins += 'Export'
plugins += 'SaveActions'
plugins += 'FileBrowser'
plugins += 'SplitWindow'

foreach plugin : plugins
    id = plugin.to_lower()
    skip_install = id.startswith('demo')
    shared_module(id, id + '.c',
                  link_with: libgeany,
                  include_directories: plugin_inc,
                  c_args: [def_cflags, '-DG_LOG_DOMAIN="'+plugin+'"'],
                  dependencies: [deps, dep_libgeany],
                  install_dir: join_paths(prefix, get_option('libdir'), 'geany'),
                  install: not skip_install)
endforeach
