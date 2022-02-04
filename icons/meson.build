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

if meson.version().version_compare('>=0.57.0')
    gnome.post_install(gtk_update_icon_cache: true)
else
    warning('You may need to run `gtk-update-icon-cache` yourself after installing')
    warning('E.g. gtk-update-icon-cache -f -t ' + join_paths(prefix, icons_dir))
endif
