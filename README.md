# 9l
A set of basic information exchange tools for building a mesh network of peer-to-peer devices based on the Yggdrasil-Network

# Important
Working in a distributed network, play by its rules. Install the client and server on your device. Remember, a holistic node is when a device is able to transmit and receive.

# What is it?
This may seem a little confusing, but it should help with shadowdowns - ideally, imagine that everyone in the city has a device-to-device connection via Yggdrasil, in which case the city will be connected into a large mesh network.

Different users have different content that they could share - it could be a book, a movie, perhaps even computing resources, but in shadowdown conditions it becomes difficult to share.

9l attempts to provide a means for the Yggdrasil mesh network to build on its connectivity to share content using the approach that each device is self-contained (both client and server at the same time).

Understanding the difficulties of maintaining a solution in working order in conditions where users become informationally cut off from solutions from the outside, it is necessary to focus on the self-organization of users for this:
- 9l is posted under 0BSD (which actually corresponds to the public domain);
- 9l is organized as a space (directory) in which work takes place - this way you can organize several independent spaces on one device;
- convergence of space 9l is designed so that on different systems the control differs as little as possible;
- use of file entities to the maximum - if a user is isolated and does not have an up-to-date solution, then he can receive a corrected executable file from another user, restoring the work of 9l simply by replacing the old executable file with a new one in his space;
- the command interface has been reduced to 9 commands - for maximum memorization;
- not a protocol - a protocol is a permanent solution, in conditions of isolation there is nothing permanent that will always work. To do this, we will divide 9l into many independent parts and unite them with an implementation bundle (which can be replaced with another at any time, the bundle simply describes how to run the parts and what data to slip into them - in the proposed implementation, the functions of the bundle are performed by mike and sally);
- using daemons as user-space-only processes;
- using Yggdrasil connectivity allows you to easily connect devices to one network.

# Unclear words
Manifest - an analogue of torrent files, contains information about the distribution, is used to indicate what needs to be downloaded;
Daemon - is simply a background process that performs one task assigned to it;
Transfer archive - tar.gz archive which contains files that need to be transferred to the receiving device.

# Funds provided by 9l
Demons:
1. 9le - remote command shell (the goal is to share resources for executing programs on remote machines);<br>
2. kick - exchange of public peers between machines (the goal is the maximum number of paths from machine A to machine B);<br>
3. acqping - ping of a remote machine which will tell you the port of the remote machine and what tool to use to establish a connection; <br>
4. callback - file sharing between devices;<br>
5. accompilce - notification about file sharing.<br>

1, 3 - not directly presented in the bundle

Note: all daemons can be used independently of each other.

Implementation-bundle:
mike - daemon server/file server, provides convenient management of daemons and stopping them (recommended in most cases);
sally - daemon launch script, defines only how daemons in the current bundle should be launched (only for deeper scripts).

Space structure:
- bin/: contains daemon executable files;
- daemon/: contains the file provision of daemons;
- files/: contains transmission archives;
- manifest/: contains manifests;
- res/: contains resources;
- script/: contains scripts, including user ones.

# Build
1. Install Tools:
Deb:
```
#   apt install build-essential cmake
```
RPM:
```
#   dnf groupinstall "Development Tools" 
#   dnf install cmake
```
How to patch this for FreeBSD?
```
#   pkg install cmake
```
Android: use Termux.
```
$   pkg install make clang cmake
```
Windows: use qmake.

2. Begin:
```
$   cd 9l/<?>/
$   mkdir build
$   cd build
$   cmake ..
$   make
```
? - is where to insert "demon name". (*C - client, *S - server)

# How to start
1. Start demon [Yggdrasil](https://yggdrasil-network.github.io).
2. Create space (run script initdirs): on Windows run initdirs/initdirs.bat, on anoher OS initdirs/initdirs.sh
3. Copy the resulting daemon executables to the bin/ directory bin/ of YOUR SPACE (dll are also needed on Windows).
4. Copy the files from the script/ directory daemon files to the script/ directory of YOUR SPACE.
5. Start the daemon server:
```
$   cd my_space/script
$   python mike.py
```
5. After the ">" symbol appears, you can enter the following commands:
	create - create a manifest file and an archive file
	start manifest - download a file based on a given manifest
	start sedder - start duplicating
	start peer server - start a node address exchange server
	start peer client - start the node address exchange client
	stop <daemon> or <all> - stopping daemons and freeing ports
	get daemon - see all daemons and their ports
	status <daemon> - information about the state of the daemon
	contact - set discovery ports my_contact
	help - command cheat sheet

	After creating the distribution with the "create" command, you need to copy the tar.gz archive to files/ (your space) and the txt file to manifest/ (your space). After this step, you can start distributing files (this is only relevant if you are creating a distribution).

	If you want to download a file (join the distribution), use the command "start manifest".

	To start distribution, use the command "start sedder".

	The server will ask you to answer interactive questions, after which the daemon will start, performing the operation you assigned to it.

	IMPORTANT: Don't forget to put the manifest in manifest/ of your space before running operations.

# File server mode

Run in file server mode (Plan9 style "Everything is a file"). For this purpose, door files (or special files) are used. These are my_space/daemon/init/initctl (when a command is written to a file, it is executed by the daemon server), my_space/daemon/daemon_status_files/<id_daemon> (by changing the entry in the daemon file, you can stop it). This has advantages if you are using 9l together with 9p, NFS, FTP, or the device you are using does not have input facilities, it can also be useful when writing scripts. For management, access to the file system is sufficient.
The commands written by initctl are listed first, after the parameters to the command are listed, each parameter must be separated by quotes and a space (format: <command> <"parameter1"> <"parameter2">) all parameters correspond to the sequence of answers to interactive questions from the daemon server.

Running in file server mode:
```
$   cd my_space/script
$   python mike.py -9
```
Using a door file using the example of starting a distribution:
```
$   echo start sedder > ../daemon/init/initctl
```
Using a door file using the example of create a distribution:
```
$   echo create '"/home/user/catal/the_memories_of_phantasm" "the_memories_of_phantasm" "the_memories_of_phantasm" "1345" "user" "ova, tv"' > ../daemon/init/initctl
```
Using the door file using the example of stopping the daemon:
```
$   echo stop > ../daemon/daemon_status_files/id_daemon
```
File server mode supports a limited number of commands: create, start manifest, start sedder, start peer server, start peer client, stop.

# Advanced use and offline use
It is possible to use daemons in isolation from the daemon server, for this, instead of mike, use sally directly, but in this case you take on the responsibility of stopping the spawned processes - this is useful if you need deeper control over actions, for example in scripts. In this case, you run the necessary script for the daemon yourself and control its termination.
Also, if necessary, it is possible to launch daemons even without sally - and by running the necessary executable file from the console with manual input - this is possible due to the fact that 9l is a set of loosely coupled programs.

# Acuping use
For auping to work, if you want to notify other devices about your ports on which you want to provide funds to others on a voluntary basis, you need to use the "contact" command in mike and run aupingS (which corresponds to the backend).
If you want to get the ports of a remote device, you need to know its IPv6 address and pass it to the input acupingC (which corresponds to the client side).

# Kick use
If you want to share the list of public peers with other devices. To get started, get a list of your peers:
```
#   yggdrasilctl getpeers > ../res/nodes
```
Start the daemon server (so that you can respond to other devices), to do this, use the "start peer server" command in mike. 

Next, you need to use the "start peer client" command, you will be asked to enter the IPv6 address and port (it is assumed that the port can be found using acuping) of the device with which you want to synchronize the addresses of peers, specify the addresses in square brackets, you can specify several addresses. Example: "[IPv6 port] [IPv6 port] [IPv6 port]". The list of peers will be available in the /res/nodes file of your space.

# Conclusion
Cons: 
- Ports, resources tend to run out.
- No mesh-oriented Wi-fi implementation: An implementation where all devices are simply connected without prior configuration is a problem that needs to be solved separately by changing or creating a new standard. 
- Crooked hands (but not everything is as good as we would like) \(T_T)/

And why is it at all?: this is an extreme decision. A VPN works as long as you don't cut off your trunk lines and don't jam satellite or mobile communication channels.

Many thanks to Chi, Chio and Cirno, Yggdrasil community, rabbits message: "No matter how you look at Glenda, it's a file".
	
# Does it work without the Internet?
Yes.
Tested on older IBSS devices. To quickly enter mesh mode, use [powermesh.sh] (https://github.com/JmpKey/siren1/blob/main/powermesh.sh) (to exit mesh mode RESTART NetworkManager).
