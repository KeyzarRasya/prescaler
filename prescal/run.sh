#!/bin/bash
set -e

SERVICE_NAME=prescal
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

echo "🔧 Building $SERVICE_NAME ..."

# Ensure build dir
mkdir -p build
cd build

# Build the project
cmake ..
make

# Copy binary
sudo cp ./prescal /usr/bin/
echo "✅ Copied prescal binary to /usr/bin"

# Create systemd service
sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=Predictive Scaling Main Engine
After=network.target

[Service]
ExecStart=/usr/bin/prescal
Restart=always
User=root
WorkingDirectory=/usr/bin
Environment=HOME=/root/

[Install]
WantedBy=multi-user.target
EOF

echo "✅ Created $SERVICE_FILE"

# Reload and enable service
sudo systemctl daemon-reload
sudo systemctl enable $SERVICE_NAME
sudo systemctl restart $SERVICE_NAME

echo "✅ Service $SERVICE_NAME started successfully!"
