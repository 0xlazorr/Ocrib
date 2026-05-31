#!/bin/bash
# Ocrib Agent macOS One-Click Installer

set -e

# Target paths
INSTALL_BIN_DIR="$HOME/.local/bin"
INSTALL_PLIST_DIR="$HOME/Library/LaunchAgents"
AGENT_WORK_DIR="/Users/Shared/Ocrib"
PLIST_NAME="com.ocrib.agent.plist"

echo "=== Installing Ocrib Agent on macOS ==="

# 1. Create directory structures
mkdir -p "$INSTALL_BIN_DIR"
mkdir -p "$INSTALL_PLIST_DIR"
mkdir -p "$AGENT_WORK_DIR"
chmod 777 "$AGENT_WORK_DIR" # Ensure write access

# 2. Compile or copy the binary
echo "Compiling the agent binary..."
if command -v gcc >/dev/null 2>&1; then
    gcc -Wall -O2 agent.c -lcurl -o "$INSTALL_BIN_DIR/remote_agent"
else
    echo "Error: gcc compiler not found. Please install Xcode Command Line Tools."
    exit 1
fi

# 3. Create the LaunchAgent Plist file
cat << EOF > "$INSTALL_PLIST_DIR/$PLIST_NAME"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.ocrib.agent</string>
    <key>ProgramArguments</key>
    <array>
        <string>$INSTALL_BIN_DIR/remote_agent</string>
        <string>127.0.0.1</string>
        <string>8080</string>
    </array>
    <key>KeepAlive</key>
    <true/>
    <key>RunAtLoad</key>
    <true/>
    <key>WorkingDirectory</key>
    <string>$AGENT_WORK_DIR</string>
    <key>StandardOutPath</key>
    <string>$AGENT_WORK_DIR/ocrib_agent.log</string>
    <key>StandardErrorPath</key>
    <string>$AGENT_WORK_DIR/ocrib_agent.err</string>
</dict>
</plist>
EOF

# 4. Stop existing agent instance if running
echo "Stopping any existing launch service..."
launchctl unload "$INSTALL_PLIST_DIR/$PLIST_NAME" >/dev/null 2>&1 || true

# 5. Load and start the launch agent
echo "Registering LaunchAgent service..."
launchctl load "$INSTALL_PLIST_DIR/$PLIST_NAME"

echo "=== Success! macOS installation complete ==="
echo "The agent will run persistently in the background on every login."
echo "Logs are available at: $AGENT_WORK_DIR/ocrib_agent.log"
EOF
