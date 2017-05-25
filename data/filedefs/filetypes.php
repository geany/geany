# For complete documentation of this file, please see Geany's main documentation
[styling=HTML]

[keywords=HTML]

[lexer_properties]
phpscript.mode=1

[settings]
# default extension used when saving files
extension=php

# MIME type
mime_type=application/x-php

# the following characters are these which a "word" can contains, see documentation
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# these comments are used for PHP, the comments used in HTML are in filetypes.xml
# single comments, like # in this file
comment_single=//
# multiline comments
comment_open=/*
comment_close=*/

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

# if this setting is set to true, a new line after a line ending with an
# unclosed tag will be automatically indented
xml_indent_tags=true

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=php -l "%f"
run_cmd=php "%f"

# use can also use something like this, to view your PHP or HTML files through a browser and webserver
#run_cmd=firefox http://localhost/test_site/%f
