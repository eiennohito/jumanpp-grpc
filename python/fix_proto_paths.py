#!/usr/bin/env python3

from sys import argv
import re

# Protubuf imports with regard to python seem to be crazy
# We want to put all protos into a module, so fix all imports

pat = re.compile(r"""import "([^"]+)";\n""")


def copy(inf, outf, prefix):
    for line in inf:
        m = pat.match(line)
        if m:
            outf.write(f"import \"{prefix}/{m.group(1)}\";\n")
        else:
            outf.write(line)


if __name__ == '__main__':
    with open(argv[1], 'rt') as inf:
        with open(argv[2], 'wt') as outf:
            copy(inf, outf, argv[3])
