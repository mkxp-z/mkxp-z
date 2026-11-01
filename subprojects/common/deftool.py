import re
import sys

input_path = sys.argv[1]
output_path = sys.argv[2]

exports_seen = False

escape_pattern = re.compile(r'^(\.[^ \t]*|[^ \t."][^ \t.]*\.[^ \t]*)($|[ \t])')

with open(input_path, 'r') as input_file, open(output_path, 'w') as output_file:
    for line in input_file:
        # Remove the first occurrence of ';' in the line and everything after it.
        semicolon_index = line.find(';')
        if semicolon_index >= 0:
            line = line[:semicolon_index]

        # Remove leading and trailing whitespace.
        line = line.strip()

        # Ignore empty lines.
        if len(line) == 0:
            continue

        # Do no processing on lines up to and including the one containing 'EXPORTS'.
        if not exports_seen:
            if line == 'EXPORTS':
                exports_seen = True
            output_file.write(line + '\n')
            continue

        original_line = line

        # If the export name contains a '.' character and isn't already escaped by surrounding it with double quotes, escape the export name by surrounding it in double quotes.
        # GCC currently outputs unescaped export names containing '.' characters when compiling with link-time optimization enabled, which causes linker errors.
        # (e.g. 'hello.lto_priv.0' -> '"hello.lto_priv.0"' and 'hello.lto_priv.0 DATA' -> '"hello.lto_priv.0" DATA')
        line = escape_pattern.sub(r'"\1"\2', line)

        print('    deftool: ' + original_line + ' -> ' + line)
        output_file.write(line + '\n')
