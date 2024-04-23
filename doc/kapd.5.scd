kapd(5)

# NAME

kapd - configuration file

# DESCRIPTION

The config file format follows a basic key-value pair format. Options are set
one per line with the syntax:

	key=value

Whitespace is ignored. Values starting or ending with whitespace can be given by enclosing them in double quotes like so:

	key=" value"

Lines beginning with # or ; are treated as comments and ignored. Section headers
are ignored. Options passed on the command line take precedence over those in the
configuration file.

# OPTIONS
All options that can be set from the command line can be set in the configuration file. The following options are available:

*database*=/path/to/database
	Specifies the path to the database file.++
	Default: $XDG_DATA_HOME/kaprica/history.db

*size*=(x)KB/MB/GB
	Specifies the size of the database file.++
	Default: 2GB

*min-length*=(x)
	Specifies the minimum length of an entry to be saved.++
	Default: 6

*expire*=(x)days
	Specifies the number of days after which an entry is deleted.++
	Default: 30 days

*limit*=(x)
	Specifies the maximum number of entries to be saved.++
	Default: 10000

# LOCATION

The following places are checked for configuration files in order:
	. The path specified by the --config option.
	. _$XDG_CONFIG_HOME/kaprica/config_
	. _$HOME/.config/kaprica/config_
	. _/etc/kaprica/config_
The configuration file is optional. If it does not exist, the default values are used.

# AUTHOR
Written by Haden Collins <collinshaden@gmail.com>, development hosted at <https://github.com/Artsy‐Macaw/kaprica>.

# SEE ALSO
*kapd*(1), *kapc*(1), *kapg*(1)
