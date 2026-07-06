import multiprocessing
import os
import time
import sys
import uuid
import re
import platform
import socket
import subprocess
import shutil
import signal
from pathlib import Path
import threading
import shlex

# For recursively killing processes
try:
    import psutil
except ImportError:
    print("[MIKE] WARNING: 'psutil' library not found")
    psutil = None

# Import from module daemon
try:
    from sally import run_downloads, run_sedder, run_search_peers_serv, run_search_peers_cli
except ImportError:
    print("[MIKE] sally.py module not found")

try:
    import readline
except ImportError:
    try:
        import pyreadline3 as readline
    except ImportError:
        readline = None

if readline:
    readline.clear_history()
    readline.set_history_length(100)

# File server mode check and locking for thread safety
FILE_SERVER_MODE = "-9" in sys.argv
state_lock = threading.RLock()

running_daemons = {}
daemon_status = {}
daemon_ports = {} # dictionary for storing daemon ports {id_daemon: [ports]}

DAEMON_STATUS_DIR = Path("../daemon/daemon_status_files")
DAEMON_STATUS_DIR.mkdir(exist_ok=True)
INITd_DIR = Path("../daemon/init")
INITd_DIR.mkdir(exist_ok=True)

available_ports = list(range(1024, 65532))

def _start_in_new_group(func, id_daemon, *args):
    if platform.system() != "Windows":
        try:
            os.setpgrp()
        except: pass
    func(id_daemon, *args)


def run_create_wrapper(id_daemon, directory, archive_name, title, portcall, author, tags):
    try:
        cmd = [sys.executable, "create.py", "-p", directory, archive_name, title, portcall, author, tags]
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = process.communicate()
        if process.returncode == 0:
            update_daemon_status(id_daemon, 'completed')
            print(f"\n[Daemon {id_daemon}] Completed")
        else:
            error_msg = stderr.strip() if stderr else "Unknown error"
            update_daemon_status(id_daemon, f'error: {error_msg[:100]}')
            print(f"\n[Daemon {id_daemon}] Error run: {error_msg}")
    except Exception as e:
        update_daemon_status(id_daemon, f'exception: {str(e)}')
        print(f"\n[Daemon {id_daemon}] Critical Error: {e}")

def get_ports_and_save():
    port_9les = input("Port for 9les = ")
    port_kickS = input("Port for kickS = ")
    if port_9les.isdigit() and port_kickS.isdigit():
        if int(use_port(port_9les)) != -1 and int(use_port(port_kickS)) != -1:
            result_string = f"PORTS: [9les->{port_9les} kickS->{port_kickS}]"
            file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../res/my_contact')
            with open(file_path, 'w') as file:
                file.write(result_string + '\n')
            print("OK")
        else:
            print("Error: Port used")
    else:
        print("Error: Incorrect data entered")

def clear_directory_contents(relative_path):
    parent_directory = os.path.abspath(os.path.join(relative_path, os.pardir))
    target_directory = os.path.join(parent_directory, relative_path)
    if os.path.exists(target_directory) and os.path.isdir(target_directory):
        for item in os.listdir(target_directory):
            item_path = os.path.join(target_directory, item)
            if os.path.isdir(item_path): shutil.rmtree(item_path)
            else: os.remove(item_path)
        print(f"Dir '{target_directory}' clear.")

def find_string_in_file(file_path, search_string):
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            for line in file:
                if search_string in line:
                    hash_index = line.find('#')
                    quote_index = line.find('"', hash_index)
                    if hash_index != -1 and quote_index != -1:
                        return line[hash_index + 1:quote_index].strip()
        return None
    except: return None

def get_ipv6_addresses_list():
    os_type = platform.system().lower()
    addresses = []
    if os_type != 'windows':
        try:
            if os_type == 'linux': cmd = "ip -6 addr show scope global | grep inet6 | awk '{print $2}' | cut -d/ -f1"
            else: cmd = "ifconfig | grep 'prefixlen 7' | awk '{print $2}'"
            result = subprocess.check_output(cmd, shell=True, text=True)
            addresses = [addr.strip() for addr in result.splitlines() if addr.strip()]
        except: pass
    else:
        try:
            addr_info = socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET6)
            for item in addr_info:
                ip = item[4][0]
                if ip.count(':') == 7: addresses.append(ip)
        except: pass
    return list(set(addresses))

def use_port(port_a, port_b=None):
    global available_ports
    with state_lock:  # Synchronizing port access
        try:
            port_a = int(port_a)
            if port_b is None:
                if port_a in available_ports:
                    available_ports.remove(port_a)
                    return port_a
                return -1
            else:
                port_b = int(port_b)
                if port_a in available_ports and port_b in available_ports:
                    if port_b == port_a + 1:
                        available_ports.remove(port_a)
                        available_ports.remove(port_b)
                        return (port_a, port_b)
                return -1
        except: return -1

def get_size_line(filename):
    try:
        with open(filename, 'r', encoding='utf-8') as file:
            lines = file.readlines()
            if len(lines) >= 3:
                size_line = lines[2].strip()
                size_value = size_line.split('"')[1]
                return int(size_value.split()[0])
    except: return None

def find_matching_files(dir_a, dir_b):
    matching_files = []
    if not dir_a.exists() or not dir_b.exists(): return []
    for filename in os.listdir(dir_a):
        if filename.endswith('.txt'):
            file_a_path = dir_a / filename
            size_a = get_size_line(file_a_path)
            corresponding_filename = filename.replace('.txt', '.tar.gz')
            file_b_path = dir_b / corresponding_filename
            if file_b_path.exists():
                size_b = os.path.getsize(file_b_path)
                if size_a is not None and size_a == size_b:
                    matching_files.append(filename)
    return matching_files

def generate_unique_id(): return str(uuid.uuid4())

def get_daemon_status_file(id_daemon): return DAEMON_STATUS_DIR / id_daemon

def update_daemon_status(id_daemon, status):
    with state_lock:
        daemon_status[id_daemon] = status
        status_file = get_daemon_status_file(id_daemon)
        try:
            with open(status_file, "w") as f: f.write(status)
        except: pass

def start_daemon_process(daemon_func, id_daemon, ports, *args):
    # ports: list of ports this daemon has occupied (e.g. [1024, 1025])
    
    with state_lock:  # Synchronization
        try:
            process = multiprocessing.Process(target=_start_in_new_group, args=(daemon_func, id_daemon, *args))
            process.start()
            running_daemons[id_daemon] = process
            daemon_ports[id_daemon] = ports # Save ports
            update_daemon_status(id_daemon, 'running')
            print(f"Daemon {id_daemon} run (PID: {process.pid}) on ports: {ports}")
        except Exception as e:
            print(f"Error run {id_daemon}: {e}")
            update_daemon_status(id_daemon, 'error')

def stop_daemon_process(id_daemon):
    global available_ports
    with state_lock:
        if id_daemon in running_daemons:
            # Freeing ports before deleting
            if id_daemon in daemon_ports:
                freed_ports = daemon_ports[id_daemon]
                for p in freed_ports:
                    if p not in available_ports:
                        available_ports.append(p)
                available_ports.sort() # Keep the list sorted
                print(f"[MIKE] Freed ports for {id_daemon}: {freed_ports}")
                del daemon_ports[id_daemon]

            process = running_daemons[id_daemon]
            pid = process.pid
            try:
                if platform.system() != "Windows":
                    try:
                        pgid = os.getpgid(pid)
                        os.killpg(pgid, signal.SIGKILL)
                    except:
                        process.terminate()
                else:
                    if psutil:
                        try:
                            parent = psutil.Process(pid)
                            for child in parent.children(recursive=True):
                                child.kill()
                            parent.kill()
                        except: pass
                    else:
                        process.kill()
                
                process.join(timeout=1)
                
                if id_daemon in running_daemons: 
                    del running_daemons[id_daemon]
                update_daemon_status(id_daemon, 'stopped')
                print(f"Daemon {id_daemon} stop")
            except Exception as e:
                print(f"Stop error {id_daemon}: {e}")
                update_daemon_status(id_daemon, 'error')
        else:
            print(f"[MIKE] Daemon {id_daemon} not found in active")

def get_daemon_list():
    with state_lock:
        status_list = []
        all_ids = set(list(running_daemons.keys()) + list(daemon_status.keys()))
        for daemon_id in all_ids:
            status = daemon_status.get(daemon_id, 'unknown')
            ports = daemon_ports.get(daemon_id, [])
            status_list.append({'id_daemon': daemon_id, 'status': status, 'ports': ports})
        return status_list

def get_daemon_error_status(id_daemon):
    status_file = get_daemon_status_file(id_daemon)
    if not status_file.exists(): return "Status not found"
    try:
        with open(status_file, "r") as f:
            return f"Status: {f.read().strip()}"
    except: return "Error read"

# executing commands in file server mode (initctl)
def execute_non_interactive_command(parts):
    if not parts:
        return
    cmd = parts[0]
    args = parts[1:]

    with state_lock:
        if cmd == "create":
            if len(args) >= 6:
                directory, archive_name, title, portcall, author, tags = args[:6]
                id_daemon = f"create_{generate_unique_id()[:8]}"
                start_daemon_process(run_create_wrapper, id_daemon, [], directory, archive_name, title, portcall, author, tags)
            else:
                print("[INITCTL] Error: 'create' requires 6 arguments: <directory> <archive_name> <title> <portcall> <author> <tags>")
        
        elif cmd == "start":
            if not args:
                print("[INITCTL] Error: 'start' command requires daemon type (sedder, manifest, peer)")
                return
            d_type = args[0]
            
            if d_type == "sedder":
                prompt_for_parameters("start sedder")
            
            elif d_type == "manifest":
                if len(args) >= 4:
                    name_manifest, u_port, downloads_out = args[1:4]
                    parent_directory = Path.cwd().parent
                    manifest_path = parent_directory / "manifest" / f"{name_manifest}.txt"
                    if manifest_path.exists():
                        try:
                            p = int(u_port)
                            if use_port(p, p+1) != -1:
                                id_daemon = f"manifest_{generate_unique_id()[:8]}"
                                tar_file = parent_directory / "files" / f"{name_manifest}.tar.gz"
                                start_daemon_process(run_downloads, id_daemon, [p, p+1], manifest_path, str(p), downloads_out, str(tar_file))
                            else:
                                print(f"[INITCTL] Error: Port {p} or {p+1} is already in use.")
                        except ValueError:
                            print("[INITCTL] Error: Port must be an integer.")
                    else:
                        print(f"[INITCTL] Error: Manifest path does not exist: {manifest_path}")
                else:
                    print("[INITCTL] Error: 'start manifest' requires 3 arguments: <name_manifest> <port> <directory>")
            
            elif d_type == "peer" and len(args) >= 3:
                sub_type = args[1]
                if sub_type == "server":
                    u_port = args[2]
                    try:
                        p = int(u_port)
                        if use_port(p) != -1:
                            id_daemon = f"pserv_{generate_unique_id()[:8]}"
                            start_daemon_process(run_search_peers_serv, id_daemon, [p], p)
                        else:
                            print(f"[INITCTL] Error: Port {p} is already in use.")
                    except ValueError:
                        print("[INITCTL] Error: Port must be an integer.")
                elif sub_type == "client":
                    connection_string = args[2]
                    id_daemon = f"pcli_{generate_unique_id()[:8]}"
                    start_daemon_process(run_search_peers_cli, id_daemon, [], connection_string)
                else:
                    print(f"[INITCTL] Error: Unknown peer sub-type '{sub_type}'")
            else:
                print(f"[INITCTL] Error: Unknown daemon type or insufficient arguments: {' '.join(args)}")

        elif cmd == "stop" and len(args) >= 1:
            target = args[0]
            if target == "all":
                for d_id in list(running_daemons.keys()):
                    stop_daemon_process(d_id)
            else:
                stop_daemon_process(target)
        else:
            print(f"[INITCTL] Unknown command: {cmd}")

# Monitoring status files and initctl startup file
def file_monitor_loop():
    initctl_path = INITd_DIR / "initctl"
    
    # at startup the initctl file exists and is empty
    try:
        with open(initctl_path, "w", encoding="utf-8") as f:
            f.write("")
    except: pass

    while True:
        time.sleep(0.5)  # 500 ms
        
        # 1. Checking "stop" signals in daemon status files
        with state_lock:
            active_ids = list(running_daemons.keys())
        
        for daemon_id in active_ids:
            status_file = get_daemon_status_file(daemon_id)
            if status_file.exists():
                try:
                    with open(status_file, "r") as f:
                        content = f.read().strip()
                    if content == "stop":
                        print(f"\n[MONITOR] Stop command received via file for: {daemon_id}")
                        stop_daemon_process(daemon_id)
                        print("> ", end="", flush=True)
                except Exception:
                    pass

        # 2. Checking the initctl control file for start/stop commands
        if initctl_path.exists() and initctl_path.stat().st_size > 0:
            try:
                # read command
                with open(initctl_path, "r", encoding="utf-8") as f:
                    cmd_line = f.read().strip()
                
                # IMMEDIATELY clear (erase) the initctl file
                with open(initctl_path, "w", encoding="utf-8") as f:
                    f.write("")
                
                if cmd_line:
                    print(f"\n[MONITOR] Received command from initctl: {cmd_line}")
                    # Correctly split the string, storing parameters inside quotes
                    parts = shlex.split(cmd_line)
                    if parts:
                        execute_non_interactive_command(parts)
                    print("> ", end="", flush=True)
            except Exception as e:
                print(f"\n[MONITOR] Error reading/clearing initctl: {e}")
                print("> ", end="", flush=True)

def prompt_for_parameters(command_type):
    params = {}
    parent_directory = Path.cwd().parent
    if command_type == "create":
        params['id_daemon'] = f"create_{generate_unique_id()[:8]}"
        params['directory_to_zip'] = input("Enter path to the catalog: ")
        params['archive_name'] = input("Output file Name (no expansion): ")
        params['title'] = input("Hand name: ")
        params['portcall'] = input("Redundancy port: ")
        params['author'] = input("Author: ")
        params['tags_input'] = input("Tags: ")
        params['ports'] = [] # For create, ports are not reserved in available_ports
        return params
    elif command_type == "start sedder":
        matching_files = find_matching_files(parent_directory / "manifest", parent_directory / "files")
        ipv6_addresses = get_ipv6_addresses_list()
        if not ipv6_addresses: return None
        inet6 = ipv6_addresses[0]
        count = 0
        for manifest_file in matching_files:
            manifest_path = parent_directory / "manifest" / manifest_file
            myport = find_string_in_file(manifest_path, inet6)
            if myport:
                try:
                    p1 = int(myport); p2 = p1 + 1
                    if use_port(p1, p2) != -1:
                        id_daemon = f"sedder_{manifest_file.split('.')[0]}_{generate_unique_id()[:8]}"
                        # We pass the list of ports [p1, p2]
                        start_daemon_process(run_sedder, id_daemon, [p1, p2], manifest_path, myport)
                        count += 1
                except: pass
        print(f"Run daemons: {count}")
        return {"batch": True}
    elif command_type == "start manifest":
        id_daemon = f"manifest_{generate_unique_id()[:8]}"
        params['id_daemon'] = id_daemon
        name_manifest = input("Name manifest (without .txt): ")
        manifest_path = parent_directory / "manifest" / f"{name_manifest}.txt"
        if not manifest_path.exists(): return None
        params['manifest_path'] = manifest_path
        u_port = input("Port: ")
        try:
            p = int(u_port)
            if use_port(p, p+1) != -1: 
                params['port'] = str(p)
                params['ports'] = [p, p+1]
            else: return None
        except: return None
        params['downloads_out'] = input("Directory (empty - auto): ")
        tar_file = parent_directory / "files" / f"{name_manifest}.tar.gz"
        params['tar_path'] = str(tar_file)
        return params
    elif command_type == "start peer server":
        params['id_daemon'] = f"pserv_{generate_unique_id()[:8]}"
        u_port = input("Port peer server: ")
        try:
            p = int(u_port)
            if use_port(p) != -1:
                params['port'] = p
                params['ports'] = [p]
                return params
        except: pass
        return None
    elif command_type == "start peer client":
        params['id_daemon'] = f"pcli_{generate_unique_id()[:8]}"
        params['port'] = input("[<IPv6> <port>] [<IPv6> <port>] ...: ")
        params['ports'] = [] # Client usually does not bind to a fixed port from the list
        return params
    return None

def handle_command(command):
    parts = command.strip().split()
    if not parts: return
    cmd = parts[0]
    args = parts[1:]

    if cmd == "get" and len(args) > 0 and args[0] == "daemon":
        for d in get_daemon_list(): 
            p_str = f" | Ports: {d['ports']}" if d['ports'] else ""
            print(f"ID: {d['id_daemon']} | Статус: {d['status']}{p_str}")
    elif cmd == "stop":
        if not args: return
        if args[0] == "all":
            for d_id in list(running_daemons.keys()): stop_daemon_process(d_id)
        else: stop_daemon_process(args[0])
    elif cmd == "status":
        if args: print(get_daemon_error_status(args[0]))
    elif cmd == "create":
        p = prompt_for_parameters("create")
        if p and all(p[key] for key in ['id_daemon', 'directory_to_zip', 'archive_name', 'title', 'portcall', 'author', 'tags_input']):
            start_daemon_process(run_create_wrapper, p['id_daemon'], p['ports'], p['directory_to_zip'], p['archive_name'], p['title'], p['portcall'], p['author'], p['tags_input'])
        else:
            print("Error: answer all questions")
    elif cmd == "start":
        if not args: return
        d_type = args[0]
        if d_type == "sedder": prompt_for_parameters("start sedder")
        elif d_type == "manifest":
            p = prompt_for_parameters("start manifest")
            if p: start_daemon_process(run_downloads, p['id_daemon'], p['ports'], p['manifest_path'], p['port'], p['downloads_out'], p['tar_path'])
        elif d_type == "peer" and len(args) > 1:
            if args[1] == "server":
                p = prompt_for_parameters("start peer server")
                if p: start_daemon_process(run_search_peers_serv, p['id_daemon'], p['ports'], p['port'])
            elif args[1] == "client":
                p = prompt_for_parameters("start peer client")
                if p: start_daemon_process(run_search_peers_cli, p['id_daemon'], p['ports'], p['port'])
    elif cmd == "contact": get_ports_and_save()
    elif cmd == "help":
        print(f"{'CMD':<30} {'DESCRIPTION'}")
        print(f"{'create':<30} - create a manifest file and an archive file")
        print(f"{'start manifest':<30} - download a file based on a given manifest")
        print(f"{'start sedder':<30} - start duplicating")
        print(f"{'start peer server':<30} - start a node address exchange server")
        print(f"{'start peer client':<30} - start the node address exchange client")
        print(f"{'stop <daemon> or <all>':<30} - stopping daemons and freeing ports")
        print(f"{'get daemon':<30} - see all daemons and their ports")
        print(f"{'status <daemon>':<30} - information about the state of the daemon")
        print(f"{'contact':<30} - set discovery ports my_contact")
    elif cmd == "exit":
        for d_id in list(running_daemons.keys()): stop_daemon_process(d_id)
        sys.exit(0)

if __name__ == "__main__":
    ERRORd_DIR = Path("../daemon/error")
    ERRORd_DIR.mkdir(exist_ok=True)
    INITd_DIR = Path("../daemon/init")
    INITd_DIR.mkdir(exist_ok=True)
    clear_directory_contents(DAEMON_STATUS_DIR)
    multiprocessing.freeze_support()
    print("[MIKE] server started")

    # Start file monitoring with flag -9
    if FILE_SERVER_MODE:
        print("[MIKE] File server mode active (-9). File monitor started.")
        monitor_thread = threading.Thread(target=file_monitor_loop, daemon=True)
        monitor_thread.start()
    
    while True:
        try:
            cmd_input = input("> ").strip()
            if cmd_input: handle_command(cmd_input)
        except (KeyboardInterrupt, EOFError):
            handle_command("exit")
