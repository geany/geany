icon_dirs = [ '16x16', '24x24', '32x32', '48x48', 'scalable' ]

# in all resolutions
app_icon = 'geany'

# in all resolutions
action_icons = [
	'geany-build',
	'geany-close-all',
	'geany-save-all'
]

icons_dir = join_paths(get_option('datadir'), 'icons', 'hicolor')
tango_dir = join_paths(get_option('datadir'), 'icons', 'Tango')

if host_machine.system() == 'windows'
    # only install index.theme on Windows
    foreach dir : [icons_dir, tango_dir]
	install_data('index.theme', install_dir: dir)
    endforeach
endif

foreach dir : icon_dirs
    ext = dir == 'scalable' ? '.svg' : '.png'
    install_data(join_paths(dir, app_icon+ext), install_dir: join_paths(icons_dir, dir, 'apps'))
    foreach icon : action_icons
        install_data(join_paths(dir, icon+ext), install_dir: join_paths(icons_dir, dir, 'actions'))
    endforeach
    install_data(join_paths('tango', dir, 'geany-save-all'+ext), install_dir: join_paths(tango_dir, dir, 'actions'))
endforeach

# classviewer icons only in 16x16
class_icons = [
	'classviewer-class.png',
	'classviewer-macro.png',
	'classviewer-member.png',
	'classviewer-method.png',
	'classviewer-namespace.png',
	'classviewer-other.png',
	'classviewer-struct.png',
	'classviewer-var.png'
]

foreach icon : class_icons
    install_data(join_paths('16x16', icon), install_dir: join_paths(icons_dir, '16x16', 'apps'))
endforeach

# can't use gnome.post_install(gtk_update_icon_cache: true) because it only
# updates the hicolor icon theme
gtk_update_icon_cache = find_program('gtk4-update-icon-cache', 'gtk-update-icon-cache')
foreach dir : [icons_dir, tango_dir]
    path = join_paths(get_option('prefix'), dir)
    if meson.version().version_compare('>=0.57.0')
	meson.add_install_script(gtk_update_icon_cache, '-t', '-f', path, skip_if_destdir: true)
    else
	meson.add_install_script('sh', '-c', 'test -n "$DESTDIR" || "@0@" -t -f "@1@"'.format(gtk_update_icon_cache.full_path(), path))
    endif
endforeach
