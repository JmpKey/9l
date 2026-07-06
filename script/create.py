import os, hashlib, tarfile, subprocess, socket, platform, argparse, sys
from datetime import datetime

def get_ipv6_addresses():
    os_type = platform.system().lower()

    if os_type != 'windows':
        if os_type == 'linux':
            cmd = "ip a | grep inet6 | grep 'global' | awk '{print $2}' | awk -F'/' '{print $1}'"
            # exec cmd
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
                return addresses
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return ""
        elif os_type in ['openbsd', 'freebsd', 'netbsd', 'dragonflybsd', 'darwin']:  # on unix
            cmd = "ifconfig | grep 'prefixlen 7' | awk '{print $2}'"
            # exec cmd
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
                return addresses
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return ""

    if os_type == 'windows':
        addresses = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET6)
    
        ipv6_addresses = set()
        for addr in addresses:
            ipv6_address = addr[4][0]  # addr[4][0] contains the IPv6 address itself
            if ipv6_address.count(':') == 7:  # Check that the address has 7 colons
                ipv6_addresses.add(ipv6_address)
    
        return ipv6_addresses
    
def calculate_hash(file_path):
    hash_md5 = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def create_tar_gz_and_collect_info(directory, archive_name):
    tar_file_path = os.path.join(directory, archive_name)
    overall_hash = hashlib.md5()
    file_info = []

    with tarfile.open(tar_file_path, 'w:gz') as tar:
        for root, dirs, files in os.walk(directory):
            for file in files:
                file_path = os.path.join(root, file)
                # Let's skip the archive of ourselves
                if file == archive_name:
                    continue
                file_hash = calculate_hash(file_path)
                overall_hash.update(file_hash.encode())
                tar.add(file_path, arcname=os.path.relpath(file_path, directory))
                file_info.append({
                    'name': file,
                    'hash': file_hash
                })

    return overall_hash.hexdigest(), file_info

def create_info_file(directory, overall_hash, file_info, title, author, tags, arhive, fileinfo, portcall):
    info_file_path = os.path.join(directory, fileinfo)
    current_date = datetime.now().strftime("%Y-%m-%d")
    ipv6_addresses = get_ipv6_addresses()

    with open(info_file_path, 'w') as f:
        f.write(f'- Title: "{title}"\n')
        f.write(f'- Attributes:\n')
        f.write(f'  - Size: "{os.path.getsize(os.path.join(directory, arhive))} bytes"\n')
        f.write(f'  - Creation Date: "{current_date}"\n')
        f.write(f'  - Author: "{author}"\n')
        f.write('- Tags:\n')
        
        for tag in tags:
            f.write(f'  - "{tag.strip()}"\n')

        f.write('- Files:\n')

        for idx, file in enumerate(file_info, start=1):
            f.write(f'  - File {idx}:\n')
            f.write(f'      - Name: "{file["name"]}"\n')
            f.write(f'      - Hash: "{file["hash"]}"\n')

        f.write(f'- Overall Hash: "{overall_hash}"\n')
        f.write('- List of IPv6 Addresses of Seeders:\n')
        
        for ipv6 in ipv6_addresses:
            ipv6 += "#" + portcall
            f.write(f'  - "{ipv6}"\n')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Creating an archive and manifest file')
    
    parser.add_argument('-p', '--params', action='store_true', help='Use command line options')
    parser.add_argument('directory', nargs='?', type=str, help='Path to the directory to be compressed')
    parser.add_argument('archive_name', nargs='?', type=str, help='Name out file')
    parser.add_argument('title', nargs='?', type=str, help='Giveaway name')
    parser.add_argument('portcall', nargs='?', type=str, help='Port')
    parser.add_argument('author', nargs='?', type=str, help='Name author')
    parser.add_argument('tags', nargs='?', type=str, help='Tags (separated by commas)')

    args = parser.parse_args()

    try:
        if args.params:
            directory_to_zip = args.directory
            archive_name = args.archive_name
            title = args.title
            portcall = args.portcall
            author = args.author
            tags = args.tags.split(',') if args.tags else []
        else:
            directory_to_zip = input("Enter the path to the directory to be compressed: ")
            archive_name = input("Enter the name of the output files: ")
            title = input("Enter the name of the giveaway: ")
            portcall = input("Enter port: ")
            author = input("Enter name author: ")
            tags_input = input("Enter tags (separated by commas): ")
            tags = tags_input.split(',')

        fileinfo = archive_name + ".txt"
        archive_name = archive_name + ".tar.gz"

        overall_hash, file_info = create_tar_gz_and_collect_info(directory_to_zip, archive_name)
        create_info_file(directory_to_zip, overall_hash, file_info, title, author, tags, archive_name, fileinfo, portcall)

        print("The archive and information file have been successfully created.")
        sys.exit(0)  # Successful completion

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)  # Error run