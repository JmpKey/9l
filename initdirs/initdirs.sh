#!/bin/sh

# 1. We ask the user for the path to the root directory
echo "Enter the path to the root directory (default $HOME/my_space):"
read -r ROOT_DIR

# If the user does not enter a path, use the default HOME
if [ -z "$ROOT_DIR" ]; then
    ROOT_DIR="$HOME/my_space"
fi

# 2. Create a directory if it does not exist
mkdir -p "$ROOT_DIR" || { echo "ERROR: Failed to create a directory $ROOT_DIR"; exit 1; }

# 3. Let's go there
cd "$ROOT_DIR" || { echo "ERROR: Failed to navigate to directory $ROOT_DIR"; exit 1; }

# 4. Create the necessary directories
mkdir -p bin script daemon manifest files res || { echo "ERROR: Failed to create directories"; exit 1; }

# 5. Go to the bin directory
cd bin || { echo "ERROR: Failed to navigate to directory bin"; exit 1; }

# 6. Create symlinks to manifest and files
ln -s ../manifest/ manifest || { echo "ERROR: Failed to create a simlink on manifest"; exit 1; }
ln -s ../files/ files || { echo "ERROR: Failed to create a simlink on files"; exit 1; }

# 7. Create files
touch "../res/server.conf" || { echo "ERROR: Failed to create a file server.conf"; exit 1; }
touch "../res/client.conf" || { echo "ERROR: Failed to create a file client.conf"; exit 1; }
touch "../res/nodes" || { echo "ERROR: Failed to create a file nodes"; exit 1; }
touch "../res/clients" || { echo "ERROR: Failed to create a file clients"; exit 1; }
touch "../res/my_contact" || { echo "ERROR: Failed to create a file my_contact"; exit 1; }

# 8. Write string in files
echo "[nodes] = [$(realpath ../res/nodes)]" > "../res/server.conf" || { echo "ERROR: Failed to write to file server.conf"; exit 1; }
echo "[clients] = [$(realpath ../res/clients)]" >> "../res/server.conf" || { echo "ERROR: Failed to write to file server.conf"; exit 1; }
echo "[nodes] = [$(realpath ../res/nodes)]" > "../res/client.conf" || { echo "ERROR: Failed to write to file client.conf"; exit 1; }

# 9. Create symlinks on client.conf and server.conf
ln -s ../res/client.conf client.conf || { echo "ERROR: Failed to create a simlink on client.conf"; exit 1; }
ln -s ../res/server.conf server.conf || { echo "ERROR: Failed to create a simlink on server.conf"; exit 1; }
ln -s ../res/my_contact my_contact || { echo "ERROR: Failed to create a simlink on my_contact"; exit 1; }

# If all actions are successful, return OK
echo "OK"
