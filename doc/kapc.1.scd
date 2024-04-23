kapc(1)

# NAME

kapc - Interact with the Wayland clipboard and history database

# SYNOPSIS

*kapc* [command] [options...] <data>

# BASIC OPTIONS

*-h, --help*
	Show help message and exit.

*-v, --version*
	Show version information and exit.

# COMMANDS

*copy*
	Copy data passed to kapc to the Wayland clipboard.

*paste*
	Paste data from either the Wayland clipboard or the history database.

*search*
	Search through history database.

*delete*
	Delete entries from the history database.

# TERMINOLOGY

*Snippet* - A human readable string that is used to identify an entry in the
history database. Max length is 80 characters. Usually a portion or "snippet"
of the text copied.

*ID* - A unique identifier for an entry in the history database. This is
obtained using the *search* command.

*MIME type* - Indicates the format of the data being copied. This is used to
determine how the data should be pasted. For example, "text/plain" is used for
ascii text; A copied item can have multiple MIME types, each contaning
different data.

# COPY

*kapc copy* [options...] <data>

## DESCRIPTION

Copy will serve the <data> passed to it to the Wayland clipboard. If no <data>
is passed, it will copy from its standard input. If no explicit MIME type is
specified, it will auto-detect the type using the MAGIC database.

## OPTIONS

*-f, --foreground*
	Stops kapc from forking and runs in the foreground. Will block until it is
	stopped; useful for debugging.

*-n, --trim-newline*
	Trims the newline character from the end of the data.

*-i, --id*
	Search through history database for the entry with the given ID and copy it
	to the clipboard. Note you can only copy one id at a time, anything after
	the first will be ignored. The id should not be passed as an argument to
	this option, but as the last argument to kapc.

*-o, --paste-once*
	Only paste the data once and then exit. Note that the copied data still be
	saved to the history database.

*-p, --password*
	Do not save the copied data to the history database. This is done by
	advertising the "x-kde-passwordManagerHint" MIME type which informs the
	daemon that the data is sensitive and should not be saved.

*-c, --clear*
	Clear the clipboard instead of copying.

*-t, --type* <mime/type>
	Specify the MIME type of the data being copied instead of auto-detecting it.
	Note MIME types usually have a format of "type/subtype" and are case-insensitive.

*-r, --reverse-search*
	Search through history database for a given snippet and copy it to the clipboard.
	Note that this has the possibility of a collision and the wrong
	entry being copied, thus *--id* is recommended for more precise copying.
	Also note that it will only use exact matches.

*-D, --database* </path/to/database>
	Specify the file path to the history database.

# PASTE

*kapc paste* [options...]

## DESCRIPTION

Paste will write the data from the Wayland clipboard or history database to its
standard output. If an MIME type is not specified, it will auto-detect the
type to use prefering UTF-8 text over other types.

## OPTIONS

*-l, --list-types*
	Instead of pasting data, list the MIME types of the data in the clipboard.
	Note this can be combined with *--id* to list the MIME type of a specific
	entry in the history database.

*-n, --no-newline*
	Do not append a newline character to the end of the pasted data. Note if the
	output is not a terminal, this option is automatically enabled.

*-i, --id*
	Search through history database for the entry with the given ID(s) and paste them.
	Note that you can pass multiple IDs to this option, seperating them by
	either a space or a newline. The IDs should not be passed as arguments to
	this option, but as the last arguments to kapc. Can be combined *--type* to
	manually specify the MIME type of the data, will fallback to auto-detecting.
	Can also be combined with *--list-types* to list the MIME type of the entry.

*-t, --type* <mime/type>
	Specify the MIME type of the data being pasted instead of auto-detecting it.
	Note MIME types usually have a format of "type/subtype" and are case-insensitive.

*-D, --database* </path/to/database>
	Specify the file path to the history database.

# SEARCH

*kapc search* [options...] <search-term>

## DESCRIPTION

Search will search through the history database for entries that contain the
given <search-term>. It will then print the snippet of each entry that matches
the search term along with the ID of the entry to the standard output. If no
<search-term> is given, it will look at the standard input for the <search-term>.
If the <search-term> is still empty, it will print all entries in the history database
up to its *--limit*. All searches return the most recent entries first. By default,
searches are case-insensitive.

## OPTIONS

*-l, --limit* <number>
	Limit the number of entries returned by the search. By default, it will
	return all entries that match the search term.

*-i, --id*
	Print only the ID of the entries that match the search term. It will be
	formatted as "ID: <id>" with each ID on a new line. In combination with
	*--list*, it will be formatted as "<id>" with each ID on a new line. Cannot
	be combined with *--snippet*.

*-s, --snippet*
	Print only the snippet of the entries that match the search term. It will be
	formatted as "<snippet>..." with each snippet on a new line. In
	combination with *--list*, it will be formatted as "<snippet>" with each
	snippet on a new line. Cannot be combined with *--id*.

*-l, --list*
	Output the search results in a more compact format. This will print the ID
	and snippet of each entry on the same line, seperated by a tab. In combination
	with *--id* or *--snippet*, it will only print the ID or snippet respectively.

*-t, --type*
	Search through the history database for entries with the given MIME type(s).
	Cannot be combined with *--glob*.

*-g, --glob*
	Search through the history database for entries that match the given glob
	pattern. Note that this is case-sensitive. Note you will likely need to
	escape the glob pattern to prevent the shell from expanding it. Cannot be
	combined with *--type*.

*-D, --database* </path/to/database>
	Specify the file path to the history database.

# DELETE

*kapc delete* [options...] <search-term>

## DESCRIPTION

Delete will search through the history database for entries that contain the
given <search-term>. It will then show the snippet of each entry that matches
the <search-term> and ask for confirmation to delete the entry. The <search-term>
functions the same as in the *search* command.

## OPTIONS

*-a, --accept*
	Automatically accept the deletion of all entries that match the search term.
	Note that this will not ask for confirmation for each entry.

*-l, --limit* <number>
	Limit the number of entries deleted. By default, it will delete all
	entries that match the search term.

*-i, --id*
	Delete entries in the history database with the given ID(s). Note that you
	can pass multiple IDs to this option, seperating them by either a space or
	a newline. The IDs should not be passed as arguments to this option, but as
	the last arguments to kapc. Cannot be combined with *--glob* or *--type*.

*-t, --type*
	Delete entries the history database with the given MIME type(s). Cannot
	be combined with *--glob*.

*-g, --glob*
	Delete entries in the history database that match the given glob
	pattern. Note that this is case-sensitive. Note you will likely need to
	escape the glob pattern to prevent the shell from expanding it. Cannot be
	combined with *--type*.

*-D, --database* </path/to/database>
	Specify the file path to the history database.

# ENVIRONMENT

## WAYLAND_DEBUG

Enable debugging output when set to 1. This will cause the *wayland-client*(7)
libray to print debug messages to stderr.

## XDG_DATA_HOME

Specify the directory where the history database is stored. If not set, it
will default to "$HOME/.local/share/kaprica".

# AUTHOR
Written by Haden Collins <collinshaden@gmail.com>, development hosted at <https://github.com/Artsy‐Macaw/kaprica>.

# SEE ALSO
*kapd*(1), *kapd*(5), *kapg*(1)
