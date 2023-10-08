install_subdir('templates',    install_dir: cdata.get('GEANY_DATA_DIR'))
install_subdir('tags',         install_dir: cdata.get('GEANY_DATA_DIR'))
install_subdir('colorschemes', install_dir: cdata.get('GEANY_DATA_DIR'))

install_subdir('filedefs',     install_dir: cdata.get('GEANY_DATA_DIR'), exclude_files: 'filetypes.python.in')

configure_file(input: 'filedefs/filetypes.python.in',
               output: 'filetypes.python',
               install: true,
               install_dir: cdata.get('GEANY_DATA_DIR') + '/filedefs',
               configuration: cdata)

misc = [
    'filetype_extensions.conf',
    'snippets.conf',
    'ui_toolbar.xml',
    'geany.glade',
    'geany.css',
]
install_data(sources: misc, install_dir: cdata.get('GEANY_DATA_DIR'))
