#!/bin/bash

# Set variables
IMAGE_PATH="./ubuntu-22.04-build-env-1.0.tar"  # Path to the Docker image file
IMAGE_NAME="ubuntu-22.04-build-env:1.0"       # Docker image name
DOCKER_PACKAGE="docker.io"                    # Docker package name

# Check if a mount path is provided; default to the user's home directory if not
# if [ -z "$1" ]; then
#     echo "No mount path provided. Defaulting to the user's home directory."
#     MOUNT_PATH="/home/$(whoami)/"
# else
#     MOUNT_PATH="$1"
# fi

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

# Check and enable multi-architecture support for Docker
if ! dpkg --print-architecture | grep -q "amd64"; then
    echo "Enabling AMD64 architecture support..."
    sudo dpkg --add-architecture amd64
    sudo apt-get update
fi

# Check if the image exists, and load it if not
if ! docker images --format "{{.Repository}}:{{.Tag}}" | grep -q "$IMAGE_NAME"; then
    echo "Docker image not found. Loading the image..."
    if [ -f "$IMAGE_PATH" ]; then
        echo "Image file found. Loading the image..."
        if ! docker load -i "$IMAGE_PATH"; then
            echo "Failed to load the Docker image. Please verify the image file."
            exit 1
        fi
    else
        echo "Docker image file $IMAGE_PATH not found. Please ensure the path is correct."
        exit 1
    fi
else
    echo "Docker image already exists. Skipping loading."
fi

# Start the Docker container with the specified or default mount path
echo "Starting Docker container and mounting path $MOUNT_PATH..."

#if ! docker run --rm -it --platform linux/amd64 -u $(id -u):$(id -g) -v "$MOUNT_PATH:/work_test" -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro $IMAGE_NAME /bin/bash; then
#    echo "Failed to start the Docker container. Please check the Docker configuration and the image."
#    exit 1
#fi

echo -e "1\n1\n1\n4" > aa
CONTAINER_ID=$(docker run -d --platform linux/amd64 -u $(id -u):$(id -g) -v "$MOUNT_PATH:/work_test" -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro $IMAGE_NAME sleep infinity)
docker exec "$CONTAINER_ID" sh -c "
        cd work_test && \\
        ./build.sh config < aa
"
docker stop "$CONTAINER_ID"
docker rm "$CONTAINER_ID"