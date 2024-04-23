kapg(1)

# NAME
kapg - A GUI for interacting with the clipboard history

# SYNOPSIS

*kapg* [options...]

# DESCRIPTION

*kapg* is a GUI for interacting with the history database created by *kapd*(1). It
allows you to view the history, search for specific entries, and copy entries
to the clipboard.

# KEYBINDINGS

*ALT+ESC*
	Quit the application.

# OPTIONS

*-h, --help*
	Show help message and exit.

*-v, --version*
	Show version information and exit.

*-n, --no-csd*
	Disable close window button at the top right corner of the window.

*-s, --style* </path/to/style.css>
	Load a custom CSS stylesheet from the specified path.

*-D, --database* </path/to/database>
	Specify the file path to the history database.

# AUTHOR
Written by Haden Collins <collinshaden@gmail.com>, development hosted at <https://github.com/Artsy‐Macaw/kaprica>.

# SEE ALSO
*kapd*(1), *kapc*(1), *kapd*(5)
