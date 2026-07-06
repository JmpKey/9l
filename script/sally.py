import os
import subprocess
import sys
import argparse
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

def log_error(e, daemon_status_file, message):
    try:
        with open(daemon_status_file, "a") as f:
            f.write(f"{message}: {e}\n")
    except Exception as ex:
        print(f"Logging error: {ex}")

def run_downloads(id_daemon, manifest_path, port, downloads_out, tar_path):
    current_script_dir = Path(__file__).resolve().parent
    base_dir = current_script_dir.parent
    
    daemon_dir = base_dir / "daemon"
    daemon_dir.mkdir(exist_ok=True)
    daemon_status_file = daemon_dir / "error" / id_daemon
    
    try:
        with open(daemon_status_file, "w") as f:
            f.write("") 
    except IOError:
        pass

    bin_dir = base_dir / "bin"
    
    try:
        devnull_handle = open(os.devnull, 'wb')
    except Exception:
        return

    if os.name != 'nt':
        callback_cmd = "./callbackC"
    else:
        callback_cmd = "callbackC.exe"
    
    # 1. Wait loop callbackC
    while True:
        try:
            result = subprocess.run(
                [callback_cmd, manifest_path],
                cwd=bin_dir,
                shell=(os.name == 'nt'),
                stdout=devnull_handle,
                stderr=devnull_handle
            )
            if result.returncode == 0:
                break
        except Exception as e:
            log_error(e, daemon_status_file, 'run_downloads: error run callbackC')
        time.sleep(1)

    script_dir = base_dir / "script"

    if downloads_out == "":
        temp = manifest_path.name
        temp1 = temp.split('.')[0]
        home_directory = os.path.expanduser("~")
        downloads_out = os.path.join(home_directory, temp1)
    else:
        temp = manifest_path.name
        temp1 = temp.split('.')[0]
        downloads_out = os.path.join(downloads_out, temp1)

    if not os.path.exists(downloads_out):
        try: os.makedirs(downloads_out)
        except: pass

    if not tar_path:
        log_error("", daemon_status_file, 'run_downloads: error tar_path name null on dearhive.py')

    # 2. Run dearhive.py
    dearhive_cmd = [sys.executable, "dearhive.py", "-p", tar_path, manifest_path, downloads_out, port]
    
    exit_code = 1 # Process state variable
    try:
        time.sleep(1)
        result_hive = subprocess.run(dearhive_cmd, cwd=script_dir, stdout=devnull_handle, stderr=devnull_handle)
        exit_code = result_hive.returncode
    except Exception as e:
        log_error(e, daemon_status_file, 'run_downloads: error run dearhive.py')
        exit_code = 1

    # 3. If dearhive completed successfully (0), move on
    if exit_code == 0:
        if os.name != 'nt':
            accomplice_cmd = "./accompliceC"
        else:
            accomplice_cmd = "accompliceC.exe"
        
        accomplice_path = bin_dir / accomplice_cmd
        if accomplice_path.exists():
            try:
                relative_manifest_path_from_bin = Path(manifest_path)
                result_accompliceC = subprocess.run(
                    [accomplice_cmd, str(relative_manifest_path_from_bin)],
                    cwd=bin_dir,
                    shell=(os.name == 'nt'),
                    stdout=devnull_handle,
                    stderr=devnull_handle
                )
                
                exit_code = result_accompliceC.returncode
                
                # 4. If accompliceC completed successfully, launch the server part
                if exit_code == 0:
                    if os.name != 'nt':
                        accompliceS_cmd = "./accompliceS"
                        callbackS_cmd = "./callbackS"
                    else:
                        accompliceS_cmd = "accompliceS.exe"
                        callbackS_cmd = "callbackS.exe"
                    
                    try:
                        port_next = str(int(port) + 1)
                        proc_accompliceS = subprocess.Popen(
                            [accompliceS_cmd, str(relative_manifest_path_from_bin), port_next],
                            cwd=bin_dir,
                            shell=(os.name == 'nt'),
                            stdout=devnull_handle,
                            stderr=devnull_handle
                        )
                        
                        proc_callbackS = subprocess.Popen(
                            [callbackS_cmd, port],
                            cwd=bin_dir,
                            shell=(os.name == 'nt'),
                            stdout=devnull_handle,
                            stderr=devnull_handle
                        )
                        
                        if proc_accompliceS.poll() is None and proc_callbackS.poll() is None:
                            exit_code = 0
                        else:
                            exit_code = 1
                            log_error("Failed to keep processes alive", daemon_status_file, "run_downloads: accompliceS/callbackS died immediately")
                            
                    except Exception as e:
                        exit_code = 1
                        log_error(e, daemon_status_file, 'run_downloads: error run secondary commands')
                else:
                    log_error(f"Return code {exit_code}", daemon_status_file, "run_downloads: accompliceC failed")
            except Exception as e:
                exit_code = 1
                log_error(e, daemon_status_file, 'run_downloads: accompliceC exception')
    else:
        log_error(f"Return code {exit_code}", daemon_status_file, "run_downloads: dearhive.py failed, stopping chain")
    
    devnull_handle.close()

def run_sedder(id_daemon, manifest_path, port):
    current_script_dir = Path(__file__).resolve().parent
    base_dir = current_script_dir.parent
    daemon_status_file = base_dir / "daemon" / "error" / str(id_daemon)
    bin_dir = base_dir / "bin"
    
    try:
        devnull_handle = open(os.devnull, 'wb')
    except: return

    if os.name != 'nt':
        accompliceS_cmd, callbackS_cmd = "./accompliceS", "./callbackS"
    else:
        accompliceS_cmd, callbackS_cmd = "accompliceS.exe", "callbackS.exe"

    try:
        port_next = str(int(port) + 1)
    except: return

    def run_command(command, args, cwd, shell):
        try:
            result = subprocess.run(
                [command] + args,
                cwd=cwd,
                shell=shell,
                stdout=devnull_handle,
                stderr=devnull_handle
            )
            return result.returncode
        except:
            return 1

    # Run proc
    with ThreadPoolExecutor(max_workers=2) as executor:
        f1 = executor.submit(run_command, accompliceS_cmd, [manifest_path, port_next], bin_dir, os.name == 'nt')
        f2 = executor.submit(run_command, callbackS_cmd, [port], bin_dir, os.name == 'nt')
        
        r1, r2 = f1.result(), f2.result()

    # result
    exit_code = 0 if r1 == 0 and r2 == 0 else 1
    
    if exit_code != 0:
        if r1 != 0:
            log_error(f"Code {r1}", daemon_status_file, "run_sedder: error run accompliceS")
        if r2 != 0:
            log_error(f"Code {r2}", daemon_status_file, "run_sedder: error run callbackS")
            
    devnull_handle.close()

def run_search_peers_serv(id_daemon, port):
    current_script_dir = Path(__file__).resolve().parent
    base_dir = current_script_dir.parent
    daemon_status_file = base_dir / "daemon" / "error" / id_daemon
    
    exit_code = 1
    try:
        devnull_handle = open(os.devnull, 'wb')
        os.chdir(base_dir)
        pycmd = "python" if os.name == 'nt' else ("python3.11" if sys.platform.startswith('freebsd') or sys.platform.startswith('openbsd') or sys.platform.startswith('netbsd') or sys.platform.startswith('dragonflybsd') or sys.platform.startswith('darwin') else "python3")

        # 1. Run seenodes.py
        res_nodes = subprocess.run([pycmd, "script/seenodes.py", "-a", "res/nodes", "res/nodes"], stdout=devnull_handle, stderr=devnull_handle)
        
        if res_nodes.returncode == 0:
            bin_dir = base_dir / "bin"
            os.chdir(bin_dir)

            # 2. Run nodup.py and kickS
            p1 = subprocess.Popen([pycmd, "../script/nodup.py", "../res/nodes", "../res/nodes"], stdout=devnull_handle, stderr=devnull_handle)
            
            kickS_cmd = "kickS.exe" if os.name == 'nt' else "./kickS"
            p2 = subprocess.Popen([kickS_cmd, str(port)], stdout=devnull_handle, stderr=devnull_handle)

            p1.wait()
            p2.wait()
            
            exit_code = 0 if p1.returncode == 0 and p2.returncode == 0 else 1
            if exit_code != 0:
                log_error(f"p1:{p1.returncode} p2:{p2.returncode}", daemon_status_file, "run_search_peers_serv: nodup or kickS failed")
        else:
            exit_code = 1
            log_error(f"Code {res_nodes.returncode}", daemon_status_file, "run_search_peers_serv: seenodes.py failed")

    except Exception as e:
        exit_code = 1
        log_error(e, daemon_status_file, 'run_search_peers_serv: general error')
    finally:
        try: devnull_handle.close()
        except: pass

def run_search_peers_cli(id_daemon, addr_port):
    current_script_dir = Path(__file__).resolve().parent
    base_dir = current_script_dir.parent
    daemon_status_file = base_dir / "daemon" / "error" / id_daemon
    
    exit_code = 1
    try:
        devnull_handle = open(os.devnull, 'wb')
        bin_dir = base_dir / "bin"
        kickC_cmd = "kickC.exe" if os.name == 'nt' else "./kickC"
        
        result = subprocess.run([kickC_cmd, addr_port], cwd=bin_dir, shell=(os.name == 'nt'), stdout=devnull_handle, stderr=devnull_handle)
        exit_code = result.returncode
        
        if exit_code != 0:
            log_error(f"Code {exit_code}", daemon_status_file, "run_search_peers_cli: kickC failed")
            
    except Exception as e:
        exit_code = 1
        log_error(e, daemon_status_file, 'run_search_peers_cli error')
    finally:
        try: devnull_handle.close()
        except: pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('flag', choices=['d', 's', 'l', 'c'])
    parser.add_argument('id_daemon')
    parser.add_argument('manifest_path')
    parser.add_argument('port')
    parser.add_argument('downloads_out')
    parser.add_argument('tar_path')
    args = parser.parse_args()

    if args.flag == 'd': run_downloads(args.id_daemon, args.manifest_path, args.port, args.downloads_out, args.tar_path)
    elif args.flag == 's': run_sedder(args.id_daemon, args.manifest_path, args.port)
    elif args.flag == 'l': run_search_peers_serv(args.id_daemon, args.port)
    elif args.flag == 'c': run_search_peers_cli(args.id_daemon, args.port)

if __name__ == "__main__":
    main()
