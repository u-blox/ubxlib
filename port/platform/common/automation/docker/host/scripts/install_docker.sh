#!/bin/bash
set -e

if docker -v &>/dev/null ; then
    read -p "Docker found on system already, do you want to re-install it? (y/n) " RESP
    if [ ! "$RESP" = "y" ]; then
        echo "Skipping Docker installation..."
        exit 0
    fi
    echo "Uninstalling old Docker versions..."
    sudo apt-get remove docker docker-engine docker.io containerd runc
fi

echo "Install Docker repo..."
sudo apt-get update
sudo apt-get install \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg \
    lsb-release
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo \
  "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

echo "Install Docker..."
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io

echo "Post installation..."
sudo groupadd -f docker
# Add docker group to current user
sudo usermod -aG docker $USER

echo "Configure Docker to start on boot"
sudo systemctl enable docker.service
sudo systemctl enable containerd.service

echo "Installing docker-compose"
sudo curl -L "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
