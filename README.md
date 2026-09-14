# La Marzocco Display

<p align="center">
  <img src="readme-image.png" alt="La Marzocco Display">
</p>

A real-time display application for La Marzocco espresso machines, showing boiler temperatures, brewing status, and machine telemetry on an AMOLED screen.

## Features

- Real-time monitoring of La Marzocco espresso machine status (there is some delay caused by the architecture of LaMarzocco)
- Display boiler temperatures and brewing information
- WebSocket connection for "live" updates
- Web-based configuration interface
- Auto-reconnect functionality

## License

This project is **open source** and **free** to use, modify, and distribute.

## Supported Hardware

This project is designed for the [LilyGo T-Display S3 AMOLED](https://lilygo.cc/products/t-display-s3-amoled).

### Hardware Specifications:
- ESP32-S3 microcontroller
- 1.91" AMOLED display (240x536)
- WiFi connectivity
- USB-C interface

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (either standalone or as a VS Code extension)
- USB-C cable for connecting the display
- La Marzocco espresso machine with cloud connectivity

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/TobiKr/LaMarzocco-Display
   cd LaMarzocco-Display
   ```

2. **Open the project**
   - If using VS Code: Open the project folder in VS Code with PlatformIO extension installed
   - If using PlatformIO CLI: Navigate to the project directory

3. **Build the project**
   ```bash
   pio run
   ```
   Or click the "Build" button in the PlatformIO toolbar in VS Code.

4. **Connect your T-Display S3 AMOLED**
   - Connect the device to your computer via USB-C cable
   - The device should be recognized automatically

5. **Upload to the device**
   ```bash
   pio run --target upload
   ```
   Or click the "Upload" button in the PlatformIO toolbar in VS Code.

6. **Upload Filesystem Image**
   ```bash
   pio run --target uploadfs
   ```
   Or click the "Upload Filesystem Image" button in the PlatformIO toolbar in VS Code.
   
7. **Configure WiFi and La Marzocco credentials**
   - After first boot, the device will create a WiFi access point named `shottimer`
   - The access point is WPA2 protected. The device generates its own key on first
     boot and shows it on the setup screen, below the network name and the URL
     (the key is also printed to the serial monitor). It stays the same across
     reboots.
   - Connect to the AP and configure your WiFi credentials and La Marzocco account details via the web interface
   - Press **Confirm** on the status page when you are done. The device restarts
     and applies the settings; if you leave without confirming, it applies them
     on its own once nothing is connected to the portal any more.

> If the configuration pages show an error about a missing web interface, or the
> serial monitor reports `SPIFFS mount failed`, step 6 was skipped - the firmware
> and the web interface are flashed separately.

### Monitoring Serial Output

To view debug output and monitor the device:
```bash
pio device monitor
```
Or click the "Serial Monitor" button in the PlatformIO toolbar.

## Configuration

The device provides a web interface for configuration. After connecting to your WiFi network, you can access the configuration page to set up:
- WiFi credentials
- La Marzocco account information
- Display preferences

### Setup access point

The configuration portal carries your WiFi password and your La Marzocco account
password, so the access point serving it uses WPA2 rather than being open: on an
open network there is no link layer encryption and anything in range can read
those forms off the air.

The key is generated per device from the hardware RNG, stored in NVS and shown on
the setup screen. `AP_PASSWORD_LENGTH` and `AP_PORTAL_TIMEOUT_MS` in
`include/config.h` control its length and how long the portal waits before
applying saved settings by itself.

### Brewing simulation

Pulling GPIO 15 low fakes a brewing cycle on the display, which is useful when
working on the UI without a machine. It is off unless the build defines
`BREWING_SIM_ENABLED`, so nothing else wired to that pin can trigger it:

```ini
build_flags =
    ${env.build_flags}
    -D BREWING_SIM_ENABLED
```

Note that the portal is plain HTTP inside that WPA2 network. TLS would need a
self-signed certificate, which browsers warn about and which an attacker on the
same network can trivially substitute, and it would break the captive portal
redirect - so the link layer is the useful place to encrypt here.

### TLS certificate verification

Connections to the La Marzocco cloud (REST API and WebSocket) verify the server
certificate against the root CAs embedded in `src/lamarzocco_tls.cpp`, and the
hostname is checked as well. Because certificate dates are part of that check,
the firmware waits for an NTP sync (`TIME_SYNC_TIMEOUT_MS` in `include/config.h`)
before the first request.

`lion.lamarzocco.io` is served from AWS, so the store contains the four Amazon
roots and nothing else. Certificates from any other public CA are rejected, which
is the point of the check. The leaf certificate and the AWS Certificate Manager
intermediate below the root rotate on their own and are not pinned.

You can check the chain your device will see with:

```bash
openssl s_client -showcerts -servername lion.lamarzocco.io \
  -connect lion.lamarzocco.io:443 </dev/null 2>/dev/null \
  | grep -E "^ *[0-9]+ (s|i):"
```

If La Marzocco ever moves off AWS, the handshake fails and the device can no
longer reach the cloud. The fix is to add the new provider's root certificate to
`src/lamarzocco_tls.cpp` in PEM form. As an emergency fallback you can build with
verification disabled:

```ini
build_flags =
    ${env.build_flags}
    -D LM_TLS_INSECURE
```

This restores the previous behaviour, in which any certificate is accepted and the
connection can be intercepted. Use it only to get a device working again, not as a
permanent setting.

## Continuous Integration

Every push and pull request builds the firmware and the filesystem image through
`.github/workflows/build.yml`, and uploads both as artifacts. This catches build
breakage without a device attached; anything touching the display, the access
point or the cloud connection still needs to be tried on real hardware.

## Contributing

**Developers wanted!** We're looking for contributors to help improve this project. Whether you're interested in:
- Adding new features
- Improving the UI/UX
- Bug fixes and optimizations
- Documentation improvements
- Supporting additional hardware

Your contributions are welcome! Feel free to:
- Open issues for bugs or feature requests
- Submit pull requests
- Share your ideas and improvements

## Troubleshooting

### Upload Issues
- Make sure the USB-C cable supports data transfer (not just charging)
- Try pressing the BOOT button on the device while uploading
- Check that no other program is using the serial port

### WiFi Connection Issues
- Verify your WiFi credentials are correct
- Ensure the device is within range of your WiFi network
- Check that your network supports 2.4GHz (ESP32 doesn't support 5GHz)

## Support

For questions, issues, or suggestions, please open an issue on the GitHub repository.
