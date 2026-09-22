#!/bin/bash

# Set variables
IMAGE_NAME="waf-arm64-image:v1"       # Docker image name
DOCKER_PACKAGE="docker.io"                    # Docker package name

if [ -z "$1" ]; then
    echo "No path argument provided. Please specify a path as an argument."
    echo "Usage: $0 <mount path>"
    exit 1
fi
MOUNT_PATH="$1"

# Ensure the mount path exists
if [ ! -d "$MOUNT_PATH" ]; then
    echo "The specified mount path $MOUNT_PATH does not exist. Please check the path or create the directory."
    exit 1
fi

# Check if the current user is in the Docker group
if ! groups $(whoami) | grep -q "\bdocker\b"; then
    echo "The current user $(whoami) is not in the Docker group. Adding the user to the Docker group..."
    sudo usermod -aG docker $(whoami)
    echo "The user has been added to the Docker group. Please log out and log back in, or run 'newgrp docker' to apply the changes."
    exit 0
else
    echo "The current user $(whoami) is already in the Docker group."
fi

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "Docker is not installed. Installing Docker..."
    sudo apt-get update
    if ! sudo apt-get install -y $DOCKER_PACKAGE; then
        echo "Failed to install Docker. Please check the error messages."
        exit 1
    fi
else
    echo "Docker is already installed."
fi

# Start the Docker container with the specified or default mount path
echo "Starting Docker container and mounting path $MOUNT_PATH..."

echo -e "1\n1\n1\n4" > aa
CONTAINER_ID=$(docker run -d --platform linux/arm64 -u $(id -u):$(id -g) -v "$MOUNT_PATH:/work_arm" -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro $IMAGE_NAME sleep infinity)
docker exec "$CONTAINER_ID" bash -c '
PATH=/usr/local/ninja-arm64:/opt/cmake-3.28.3/bin:$PATH
hash -r
ninja --version
cmake --version
cd /work_arm
./build.sh config < aa
'
docker stop "$CONTAINER_ID"
docker rm "$CONTAINER_ID"