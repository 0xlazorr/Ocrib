#!/bin/bash
# Ocrib Agent Debian Packaging Script

set -e

PKG_DIR="deb_build"
DEB_VERSION="1.0.0"
DEB_NAME="ocrib-agent_${DEB_VERSION}_amd64"
DEB_PATH="${PKG_DIR}/${DEB_NAME}"

echo "=== Building Ocrib Agent Debian Package ==="

# 1. Clean previous build files
rm -rf "$PKG_DIR"
mkdir -p "${DEB_PATH}/DEBIAN"
mkdir -p "${DEB_PATH}/usr/bin"
mkdir -p "${DEB_PATH}/lib/systemd/system"
mkdir -p "${DEB_PATH}/etc/default"

# 2. Compile the agent binary
echo "Compiling agent.c..."
gcc -Wall -O2 agent.c -lcurl -o "${DEB_PATH}/usr/bin/remote_agent"

# 3. Copy systemd service file
cp ocrib-agent.service "${DEB_PATH}/lib/systemd/system/"

# 4. Create default configuration file
cat << 'EOF' > "${DEB_PATH}/etc/default/ocrib-agent"
# Configuration for Ocrib Remote Agent Service
# Point to your Ocrib cloud broker IP or domain name
OCRIB_SERVER="127.0.0.1"
OCRIB_PORT="8080"
EOF

# 5. Create control file
cat << EOF > "${DEB_PATH}/DEBIAN/control"
Package: ocrib-agent
Version: ${DEB_VERSION}
Section: utils
Priority: optional
Architecture: amd64
Depends: libc6, libcurl4
Maintainer: Ocrib Core Team <support@ocrib.com>
Description: Ocrib Remote Hardware Download Agent
 A lightweight background daemon that runs on receiver devices
 to trigger and stream remote file downloads.
EOF

# 6. Create post-installation script (postinst)
cat << 'EOF' > "${DEB_PATH}/DEBIAN/postinst"
#!/bin/sh
set -e

# Create ocrib user and group if they do not exist
if ! getent group ocrib >/dev/null; then
    groupadd -r ocrib
fi

if ! getent passwd ocrib >/dev/null; then
    useradd -r -g ocrib -d /var/lib/ocrib -s /sbin/nologin -c "Ocrib Agent daemon" ocrib
fi

# Create state directory
mkdir -p /var/lib/ocrib
chown -R ocrib:ocrib /var/lib/ocrib
chmod 750 /var/lib/ocrib

# Reload systemd, enable and start service
systemctl daemon-reload || true
systemctl enable ocrib-agent.service || true
systemctl restart ocrib-agent.service || true

echo "Ocrib Agent service installed and started successfully."
EOF
chmod 755 "${DEB_PATH}/DEBIAN/postinst"

# 7. Create pre-removal script (prerm)
cat << 'EOF' > "${DEB_PATH}/DEBIAN/prerm"
#!/bin/sh
set -e

if [ "$1" = "remove" ] || [ "$1" = "deconfigure" ]; then
    echo "Stopping Ocrib Agent service..."
    systemctl stop ocrib-agent.service || true
    systemctl disable ocrib-agent.service || true
fi
EOF
chmod 755 "${DEB_PATH}/DEBIAN/prerm"

# 8. Create post-removal script (postrm)
cat << 'EOF' > "${DEB_PATH}/DEBIAN/postrm"
#!/bin/sh
set -e

if [ "$1" = "purge" ]; then
    echo "Purging configuration and data files for Ocrib Agent..."
    rm -rf /var/lib/ocrib
    rm -f /etc/default/ocrib-agent
fi

systemctl daemon-reload || true
EOF
chmod 755 "${DEB_PATH}/DEBIAN/postrm"

# 9. Build the debian package
echo "Packaging using dpkg-deb..."
dpkg-deb --build --root-owner-group "$DEB_PATH"

# 10. Copy package to root directory
cp "${DEB_PATH}.deb" ./ocrib-agent.deb
echo "=== Success! Package built: ocrib-agent.deb ==="

