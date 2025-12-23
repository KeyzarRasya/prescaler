#!/bin/bash
set -e  # stop on any error
set -u  # fail on unset variables

APP_NAME="traffic"
BUILD_DIR="build"
BIN_PATH="/usr/local/bin/$APP_NAME"
SERVICE_FILE="/etc/systemd/system/${APP_NAME}.service"

echo "🔧 Building $APP_NAME ..."

# Ensure build directory exists
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run cmake & make
cmake .. >/dev/null
make -j"$(nproc)"

# Copy binary to system path
sudo cp "./$APP_NAME" "$BIN_PATH"
sudo chmod +x "$BIN_PATH"
echo "✅ Copied binary to $BIN_PATH"

# Copy config file to /root if it doesn't exist
if [ ! -f "/root/traffic.conf" ]; then
    sudo cp "../traffic.conf" "/root/traffic.conf"
    echo "✅ Created config file at /root/traffic.conf"
else
    echo "ℹ️  Config file already exists at /root/traffic.conf (not overwriting)"
fi

# Create systemd service file
echo "🧾 Creating systemd service at $SERVICE_FILE ..."
sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=Traffic Engine Service
After=network.target

[Service]
ExecStart=$BIN_PATH
Restart=always
User=root
WorkingDirectory=$(dirname "$BIN_PATH")
StandardOutput=append:/var/log/${APP_NAME}.log
StandardError=append:/var/log/${APP_NAME}.err

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and enable service
sudo systemctl daemon-reload
sudo systemctl enable --now "$APP_NAME"

echo "✅ Service '$APP_NAME' installed and started."
echo "💡 Check logs with: sudo journalctl -u $APP_NAME -f"
