app_icons = {'16x16': ['classviewer-class.png',
                       'classviewer-macro.png',
                       'classviewer-member.png',
                       'classviewer-method.png',
                       'classviewer-namespace.png',
                       'classviewer-other.png',
                       'classviewer-struct.png',
                       'classviewer-var.png',
                       'geany.png'],
             '32x32': ['geany.png'],
             '48x48': ['geany.png']}

actions = ['geany-build.png',
           'geany-close-all.png',
           'geany-save-all.png']

act_icons = {'16x16': actions,
             '24x24': actions,
             '32x32': actions,
             '48x48': actions}

icons_dir = join_paths(get_option('datadir'), 'icons', 'hicolor')

foreach dir, icons_array : app_icons
    foreach icon : icons_array
        install_data(join_paths(dir, icon), install_dir: join_paths(icons_dir, dir, 'apps'))
    endforeach
endforeach

foreach dir, icons_array : act_icons
    foreach icon : icons_array
        install_data(join_paths(dir, icon), install_dir: join_paths(icons_dir, dir, 'actions'))
    endforeach
endforeach
