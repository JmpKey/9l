import os
import hashlib
import tarfile
import shutil
import re
import sys
import platform
import subprocess
import socket
import argparse
import random

def shuffle_ipv6_addresses(file_path):
    with open(file_path, 'r') as f:
        lines = f.readlines()

    # Find a line with IPv6 addresses
    for i, line in enumerate(lines):
        if line.strip() == "- List of IPv6 Addresses of Seeders:":
            # We save all address lines
            ipv6_addresses = []
            j = i + 1
            while j < len(lines) and lines[j].strip() != "":
                ipv6_addresses.append(lines[j])
                j += 1
            
            # Mixing addresses
            random.shuffle(ipv6_addresses)
            
            # Replace old addresses with mixed ones
            lines = lines[:i + 1] + ipv6_addresses + lines[j:]

            break

    with open(file_path, 'w') as f:
        f.writelines(lines)

def append_to_file(file_path, address, port):
    line_to_append = f'  - "{address}#{port}"\n'
    
    with open(file_path, 'a') as file:
        file.write(line_to_append)

def get_ipv6_addresses():
    os_type = platform.system().lower()

    if os_type != 'windows':
        if os_type == 'linux':
            cmd = "ip a | grep inet6 | grep 'global' | awk '{print $2}' | awk -F'/' '{print $1}'"
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
                return addresses
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return []
        elif os_type in ['openbsd', 'freebsd', 'netbsd', 'dragonflybsd', 'darwin']:  # on unix
            cmd = "ifconfig | grep 'prefixlen 7' | awk '{print $2}'"
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
                return addresses
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return []

    if os_type == 'windows':
        addresses = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET6)
    
        ipv6_addresses = set()
        for addr in addresses:
            ipv6_address = addr[4][0]  # addr[4][0] contains the IPv6 address itself
            if ipv6_address.count(':') == 7:  # Check that the address has 7 colons
                ipv6_addresses.add(ipv6_address)
    
        return list(ipv6_addresses)

def calculate_hash(file_path):
    # MD5
    hash_md5 = hashlib.md5()
    if not os.path.exists(file_path):
        return None
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def parse_info_file(info_file_path):
    # Parses a text file and returns a list of files with their hashes and the overall hash
    files_data = []
    overall_hash = None
    
    name_re = re.compile(r'- Name: "(.*)"')
    hash_re = re.compile(r'- Hash: "(.*)"')
    overall_re = re.compile(r'- Overall Hash: "(.*)"')

    try:
        with open(info_file_path, 'r', encoding='utf-8') as f:
            content = f.readlines()
            
            current_name = None
            for line in content:
                # Looking for the final hash
                ov_match = overall_re.search(line)
                if ov_match:
                    overall_hash = ov_match.group(1)
                    continue
                
                # Name file
                name_match = name_re.search(line)
                if name_match:
                    current_name = name_match.group(1)
                    continue
                
                # Looking for a hash of a specific file
                hash_match = hash_re.search(line)
                if hash_match and current_name:
                    files_data.append({'name': current_name, 'hash': hash_match.group(1)})
                    current_name = None
                    
        return files_data, overall_hash
    except Exception as e:
        print(f"Error reading manifest file: {e}", file=sys.stderr) # error write stderr
        return None, None

def unpack_and_verify(archive_path, info_file_path, extract_to, portmiltiplex):
    # Unpacks and checks integrity
    # 1. Parse manifest
    expected_files, expected_overall_hash = parse_info_file(info_file_path)
    
    if not expected_files or not expected_overall_hash:
        print("Failed to read data from .txt file.", file=sys.stderr)
        return 1

    # Create a folder for unpacking if there is none
    if not os.path.exists(extract_to):
        os.makedirs(extract_to)

    extracted_paths = []
    
    try:
        # 2. Unpacking
        print(f"Unpacking {archive_path}...")
        with tarfile.open(archive_path, 'r:gz') as tar:
            tar.extractall(path=extract_to)
            # We remember the paths of all extracted files (for deletion in case of error)
            for member in tar.getmembers():
                extracted_paths.append(os.path.join(extract_to, member.name))

        # 3. Checking hashes
        print("Checksum checking...")
        calculated_overall_hash = hashlib.md5()
        
        for file_info in expected_files:
            file_name = file_info['name']
            expected_file_hash = file_info['hash']
            
            actual_file_path = os.path.join(extract_to, file_name)
            actual_file_hash = calculate_hash(actual_file_path)
            
            # Checking the hash of a specific file
            if actual_file_hash != expected_file_hash:
                print(f"ERROR: File hash {file_name} doesn't match!", file=sys.stderr)
                raise ValueError("Hash mismatch")
            
            # Update the shared hash
            calculated_overall_hash.update(actual_file_hash.encode())

        # Checking the final hash
        if calculated_overall_hash.hexdigest() != expected_overall_hash:
            print("ERROR: The resulting hash does not match!", file=sys.stderr)
            raise ValueError("Overall hash mismatch")

        ipv6_addresses = get_ipv6_addresses()
        for address in ipv6_addresses:
            append_to_file(info_file_path, address, portmiltiplex)

        print("The check was completed successfully. All files are intact.")
        return 0

    except Exception as e:
        print(f"Process interrupted: {e}", file=sys.stderr)
        print("Deleting unpacked files...", file=sys.stderr)
        shuffle_ipv6_addresses(info_file_path)
        
        # We delete only what we managed to unpack
        for path in extracted_paths:
            if os.path.exists(path):
                if os.path.isdir(path):
                    shutil.rmtree(path)
                else:
                    os.remove(path)
        
        # If the folder becomes empty, delete it too
        if os.path.exists(extract_to) and not os.listdir(extract_to):
            os.rmdir(extract_to)
            
        return 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Unpacking the archive and verification')

    parser.add_argument('-p', '--param', nargs=4, metavar=('ARCHIVE', 'INFO_FILE', 'OUTPUT_DIR', 'PORT'), 
                        help='Parameters: archive path, information file path, decompression directory, duplication port')

    args = parser.parse_args()

    if args.param:
        home_directory = os.path.expanduser("~")
        archive_name, info_file, output_dir, portmiltiplex = args.param

    else:
        archive_name = input("Enter the path to the archive (.tar.gz): ")
        info_file = input("Enter the path to the information file (.txt): ")
        output_dir = input("Unboxing catalog: ")
        portmiltiplex = input("Prot duplication: ")

    result = unpack_and_verify(archive_name, info_file, output_dir, portmiltiplex)
    
    sys.exit(result)
