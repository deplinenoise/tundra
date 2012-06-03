#! /usr/bin/env python

import sys

input_fn = sys.argv[1]
output_fn = sys.argv[2]

# Just read all chars and flip the lowest bit.
# Put the results in a character array.
with open(input_fn, 'r') as f_in:
    with open(output_fn, 'w') as f_out:
        f_out.write('char generated_data[] = {\n')
        for line in f_in:
            for ch in line:
                if ch != '\n':
                    f_out.write('0x%02x, ' % (ord(ch) ^ 1))
                else:
                    f_out.write("'\\n', ")
            f_out.write('\n')
        f_out.write('0\n};\n')

