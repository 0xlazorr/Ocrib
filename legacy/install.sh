#!/bin/bash
# Ocrib Agent "One-Click" Installer for Linux and macOS

# Colors for pretty output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

clear
echo -e "${BLUE}===========================================${NC}"
echo -e "${BLUE}       OCRIB: REMOTE HARDWARE AGENT        ${NC}"
echo -e "${BLUE}===========================================${NC}"
echo "Setting up your device for remote downloads..."

# Check for compiler
if ! command -v gcc &> /dev/null; then
    echo "Configuring environment... (System password may be required)"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt-get update -qq && sudo apt-get install -y gcc make libcurl4-openssl-dev -qq
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "Please ensure Xcode Command Line Tools are installed."
    fi
fi

# Build
echo "Installing Ocrib components..."
make agent > /dev/null

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Ready to go!${NC}"
    echo ""
    echo -e "To link this device, run: ${BLUE}./remote_agent YOUR_SERVER_DOMAIN${NC}"
    echo "Keep the terminal open to see your 6-digit Pairing PIN."
    echo ""
    echo "Once paired, this terminal will automatically minimize and"
    echo "run in the background."
else
    echo "Something went wrong during installation."
    echo "Please ensure you have an active internet connection."
    exit 1
fi