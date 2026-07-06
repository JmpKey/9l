import argparse, random, subprocess, os, platform, socket

def parse_nodes_file(input_file, output_file):
    try:
        with open(input_file, 'r') as file:
            lines = file.readlines()

        peers = set()

        for line in lines[1:]:
            # Divide the string by spaces and extract the IP address (the fourth element)
            parts = line.split()
            if len(parts) > 3:
                ip_address = parts[3]
                if ip_address != "0s":
                    peers.add(ip_address)  # Add IP to the set

        with open(output_file, 'w') as file:
            for peer in peers:
                file.write(peer + '\n')

        print(f"Found {len(peers)} IP.'.")

    except Exception as e:
        print("Error:", e)

def remove_duplicate_lines(in_file, out_file):
    unique_lines = set()

    with open(in_file, 'r', encoding='utf-8') as infile:
        for line in infile:
            unique_lines.add(line.strip())

    with open(out_file, 'w', encoding='utf-8') as outfile:
        for line in unique_lines:
            outfile.write(line + '\n')

def shuffle_lines(in_file, out_file):
    with open(in_file, 'r', encoding='utf-8') as infile:
        lines = infile.readlines()

    random.shuffle(lines)
    
    with open(out_file, 'w', encoding='utf-8') as outfile:
        outfile.writelines(lines)

def get_ipv6_addresses_with_seven_colons():
    # We get all interfaces and their addresses
    addresses = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET6)
    
    ipv6_addresses = set()
    for addr in addresses:
        ipv6_address = addr[4][0]  # addr[4][0] contains the IPv6 address itself
        if ipv6_address.count(':') == 7:  # Check that the address has 7 colons
            ipv6_addresses.add(ipv6_address)
    
    return ipv6_addresses

def remove_my_ip_lines(input_file):

    os_type = platform.system().lower()
    
    # see your addr
    if os_type != 'windows':
        if os_type == 'linux':
            cmd = "ip a | grep inet6 | grep 'global' | awk '{print $2}' | awk -F'/' '{print $1}'"
            # exec cmd
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return
        elif os_type in ['openbsd', 'freebsd', 'netbsd', 'dragonflybsd', 'darwin']:  # on unix
            cmd = "ifconfig | grep 'prefixlen 7' | awk '{print $2}'"
            # exec cmd
            try:
                result = subprocess.check_output(cmd, shell=True, text=True)
                addresses = result.strip().splitlines()
        
            except subprocess.CalledProcessError as e:
                print(f"Error executing command: {e}")
                return

    if os_type == 'windows':
        myip = "";
        ipv6_addrs = get_ipv6_addresses_with_seven_colons()
        for ipv6 in ipv6_addrs:
            myip = ipv6
    
        # strings that do not match addresses are recorded
        with open(input_file, 'r') as infile:
            lines = infile.readlines()

        filtered_lines = [line for line in lines if line.strip() not in myip]

        with open(input_file, 'w') as outfile:
            outfile.writelines(filtered_lines)
    
def main():
    parser = argparse.ArgumentParser(description="see file")
    
    parser.add_argument('-a', '--addnodes', nargs=2, metavar=('FILE1', 'FILE2'), help='add nodes')
    parser.add_argument('-c', '--confuse', nargs=2, metavar=('FILE1', 'FILE2'), help='scatter addresses randomly')
    
    args = parser.parse_args()

    if args.addnodes:
        parse_nodes_file(args.addnodes[0], args.addnodes[1])
        remove_my_ip_lines(args.addnodes[0])
        remove_duplicate_lines(args.addnodes[0], args.addnodes[1])
        remove_my_ip_lines(args.addnodes[0])
    elif args.confuse:
        shuffle_lines(args.confuse[0], args.confuse[1])
    else:
        print("usage: seenodes.py [-a -c] [in_file] [out_file]\n\t-a [--addnodes]\n\t-c [--confuse]")

if __name__ == "__main__":
    main()
