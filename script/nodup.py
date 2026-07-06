import argparse, time, os, sys, platform

def remove_empty_first_line_nt(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
        
    if lines and lines[0].strip() == '':
        lines = lines[1:]
        
    with open(filename, 'w') as file:
        file.writelines(lines)

def check_colons_nt(input_string):
    colon_count = input_string.count(':')
    return colon_count >= 9

def remove_duplicate_lines_nt(in_file, out_file):
    unique_lines = set()

    with open(in_file, 'r', encoding='utf-8') as infile:
        for line in infile:
            unique_lines.add(line.strip())

    with open(out_file, 'w', encoding='utf-8') as outfile:
        for line in unique_lines:
            if (check_colons_nt(line) == False):
                outfile.write(line + '\n')

def remove_duplicate_lines(in_file, out_file):
    unique_lines = set()

    with open(in_file, 'r', encoding='utf-8') as infile:
        for line in infile:
            unique_lines.add(line.strip())

    with open(out_file, 'w', encoding='utf-8') as outfile:
        for line in unique_lines:
            outfile.write(line + '\n')

def remove_lines_with_ip(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
        
    lines = [line for line in lines if "IP" not in line]
    
    with open(filename, 'w') as file:
        file.writelines(lines)

def main():
    parser = argparse.ArgumentParser(description='Remove duplicate lines from a file.')
    parser.add_argument('input_file', help='Path to the input file')
    parser.add_argument('output_file', help='Path to the output file')

    args = parser.parse_args()

    os_type = platform.system().lower()

    if os_type == 'windows':
        while True:
            remove_duplicate_lines_nt(args.input_file, args.output_file)
            remove_empty_first_line_nt(args.output_file)
            remove_lines_with_ip(args.output_file)
            time.sleep(15)  # time
    else:
        while True:
            remove_duplicate_lines(args.input_file, args.output_file)
            remove_lines_with_ip(args.output_file)
            time.sleep(4)  # time

if __name__ == "__main__":
    main()
