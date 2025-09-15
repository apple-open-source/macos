#!/usr/bin/env python3
import sys

# read a .CFLAGS file and print the appropriately quoted clang command line arguments
def main():
    in_path = sys.argv[1]
    line = open(in_path).read()
    # split by " -" (with space) to avoid issue with paths that contain dashes
    dash_split = line.split(' -')
    output = []
    # change ' -DX=y z' to ' -DX="y z"'
    for i, s in enumerate(dash_split):
        if i == 0:
            continue # skip the clang executable
        if '=' in s:
            st = s.strip()
            eq_sp = st.split('=')
            if ' ' in eq_sp[1]:
                output.append(f'-{eq_sp[0]}="{eq_sp[1]}"')
                continue

        output.append(f"-{s}")
    print(" ".join(output))


if __name__ == "__main__":
    sys.exit(main())
