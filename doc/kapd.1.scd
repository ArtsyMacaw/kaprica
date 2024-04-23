kapd(1)

# NAME

kapd - A Clipboard Manager for Wayland

# SYNOPSIS

*kapd* [options...]

# DESCRIPTION

*kapd* is the daemon component of the Kaprica clipboard manager. It listens for clipboard
events and stores the clipboard history in an history database. The clipboard history can be
accessed using the *kapc*(1) command-line interface or the GUI *kapg*(1) application.

# OPTIONS

*-h, --help*
	Show help message and exit.

*-v, --version*
	Show version information and exit.

*-c, --config* </path/to/config>
	Set the path to the configuration file. Check kapd(5) for more
	information.++
	Default: _$XDG_CONFIG_HOME/kaprica/config_

*-S, --size* <(x)KB/MB/GB>
	Set the maximum size of the clipboard history. It is required to specify the
	unit of KB, MB, or GB.++
	Default: 2GB

*-D, --database* </path/to/database>
	Set the path to the database file. If the file does not exist, it will be created.++
	Default: _$XDG_DATA_HOME/kaprica/history.db_

*-m, --min-length* <0-x>
	Set the minimum length of the text to be stored in the clipboard history.++
	Default: 6 characters

*-e, --expire* <0-x>
	Set the expiration time of the clipboard history in days.++
	Default: 30 days

*-l, --limit* <0-x>
	Set the maximum number of items in the clipboard history.++
	Default: 10,000 entries

# CONFIGURATION

The following places are checked for configuration files in order:
	. The path specified by the --config option.
	. _$XDG_CONFIG_HOME/kaprica/config_
	. _$HOME/.config/kaprica/config_
	. _/etc/kaprica/config_
The configuration file is optional. If it does not exist, the default values are used.
Check kapd(5) for more information.

# HISTORY DATABASE LAYOUT

The clipboard history is stored in an SQLite database. It can be inspected using either *kapc*(1)
or an SQLite client such as *sqlite3*(1).

Each entry in the database is stored as a row with the following columns:
[[ ID
:- Timestamp
:- Thumbnail
:- Snippet
:- Hash

*ID*: The unique identifier of the entry.++
*Timestamp*: The time the entry was added to the clipboard history. Formatted "YYYY-MM-DD HH:MM:SS".++
*Thumbnail*: A thumbnail generated from the largest image in the entry. If the entry
does not contain an image, the thumbnail is left empty.++
*Snippet*: A truncated version of the text in the entry. If the entry does not contain
text, the snippet is a timestamp and the first MIME type of the entry.++
*Hash*: Hash generated from the entry's content. Used to check for duplicates.

Each entry contains one or more MIME types. The MIME types are stored in a separate table
with the following columns:
[[ ID
:- MIME Type
:- Data
:- Size

*ID*: The unique identifier of the entry.++
*MIME Type*: The MIME type of the data.++
*Data*: The data in the entry.++
*Size*: The size of the data in bytes.

# AUTHOR
Written by Haden Collins <collinshaden@gmail.com>, development hosted at <https://github.com/Artsy‐Macaw/kaprica>.

# SEE ALSO
*kapd*(5), *kapc*(1), *kapg*(1)
