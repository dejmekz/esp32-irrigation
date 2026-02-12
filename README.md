# ESP32 Irrigation Controller

Automated garden irrigation system based on ESP32-C3 with WiFi/MQTT connectivity, RTC scheduling, and remote control capabilities.

## Features

- **Automated Scheduling**: RTC-based irrigation scheduling with customizable start times
- **12 Valve Control**: Support for up to 12 irrigation valves via I2C PCF8574 expanders
- **Pump Management**: Automatic pump control with ready-state checking
- **Remote Control**: Full MQTT-based remote control and monitoring
- **LCD Display**: 20x4 character LCD showing real-time status
- **WiFi Connectivity**: Automatic connection and reconnection handling
- **OTA Updates**: Over-the-air firmware updates via esp32FOTA
- **Persistent Configuration**: NVS-based storage for schedules and valve settings
- **Hardware Watchdog**: 30-second watchdog timer for system reliability
- **Error Recovery**: Graceful handling of RTC and connectivity failures

## Hardware Requirements

### Components

- **ESP32-C3 DevKit M-1** (or compatible)
- **DS3231 RTC Module** (Real-Time Clock with INT/SQW output)
- **20x4 LCD Display** with I2C interface (address 0x27)
- **2x PCF8574 I2C GPIO Expanders** (addresses 0x38, 0x3C)
- **Relay Module** for pump control
- **12x Valve Relays** connected to PCF8574 outputs
- **Power Supply** appropriate for your setup

### Wiring

#### ESP32-C3 Connections

| ESP32 Pin | Connection |
|-----------|------------|
| GPIO 3    | DS3231 INT/SQW |
| GPIO 4    | I2C SCL |
| GPIO 5    | I2C SDA |
| GPIO 6    | Pump Relay |

#### I2C Devices

| Device | Address | Purpose |
|--------|---------|---------|
| LCD    | 0x27    | Status display |
| PCF8574 #1 | 0x38 | Valves 0-5 (Box 1,2) |
| PCF8574 #2 | 0x3C | Valves 6-11 (Box 3,4) |
| DS3231 | 0x68 | Real-time clock |

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- Git (for cloning the repository)

### Installation

1. **Clone the repository**
   ```bash
   git clone <your-repo-url>
   cd esp32-irrigation
   ```

2. **Create configuration file**
   ```bash
   cp include/config.h.example include/config.h
   ```

3. **Edit configuration**
   Open `include/config.h` and update with your settings:
   ```cpp
   #define WIFI_SSID     "your-wifi-ssid"
   #define WIFI_PASSWORD "your-wifi-password"
   #define MQTT_SERVER   "your-mqtt-broker"
   #define MQTT_USER     "mqtt-username"
   #define MQTT_PASSWORD "mqtt-password"
   ```

4. **Build and upload**
   ```bash
   pio run -t upload
   pio device monitor  # Optional: view serial output
   ```

## Configuration

### Default Schedule

The system starts with default valve schedules:
- **Start Time**: 20:00 (8:00 PM)
- **Task 0**: Valves pattern "oxo xxx xxx xxx" for 20 minutes
- **Task 1**: Valves pattern "xox xxx xxx xox" for 17 minutes
- **Task 2**: Valves pattern "xxx xxx oox xxx" for 15 minutes
- **Task 3**: Valves pattern "xxx xxx xxx oxo" for 15 minutes

These defaults are saved to NVS on first boot and can be modified via MQTT.

### Valve Patterns

- `o` = valve open
- `x` = valve closed
- Spaces are for readability and ignored

Example: "oxo xxx xxx xxx" opens valves 0 and 2, closes all others.

## MQTT Interface

### Topics

All topics follow the pattern: `irrigation/{deviceId}/...`

- `irrigation/{deviceId}/LWT` - Last Will Testament (online/offline)
- `irrigation/{deviceId}/cmnd` - Command topic (subscribe)
- `irrigation/{deviceId}/conf` - Configuration topic (subscribe)
- `irrigation/{deviceId}/state` - State and responses (publish)
- `irrigation/{deviceId}/tasks` - Task status (publish)
- `irrigation/{deviceId}/wifi` - WiFi status (publish every 10 min)

### Commands

Send JSON commands to `irrigation/{deviceId}/cmnd`:

#### Manual Valve Control

```json
{
  "cmd": "valve_control",
  "params": {
    "valves": 5,
    "duration": 10
  }
}
```
- `valves`: Bitmask or valve pattern (5 = 0b101 = valves 0 and 2)
- `duration`: Minutes to run (0 = turn off)

#### Pump Control

```json
{
  "cmd": "pump_control",
  "params": {
    "state": true
  }
}
```
- `state`: true = on, false = off

#### Task Control

Start scheduled tasks immediately:
```json
{
  "cmd": "task_start"
}
```

Stop running tasks:
```json
{
  "cmd": "task_stop"
}
```

#### System Restart

```json
{
  "cmd": "system_restart",
  "params": {
    "delay": 5
  }
}
```
- `delay`: Seconds before restart (1-60, default 5)

### Status Messages

The system publishes status to `irrigation/{deviceId}/tasks`:

```json
{
  "time": "12.02.2026 20:15:30",
  "at": "20:00",
  "run": true,
  "pump": true,
  "valve": {
    "valves": 2560,
    "duration": 20,
    "left": 18
  }
}
```

## LCD Display

The 20x4 LCD shows:
- **Row 1**: Task status or wait message
- **Row 2**: Active valve pattern
- **Row 3**: Current date and time
- **Row 4**: RTC temperature

## Troubleshooting

### WiFi won't connect

1. Check SSID and password in `config.h`
2. Verify ESP32 is within WiFi range
3. Check serial monitor for connection attempts
4. WiFi auto-reconnects every 60 seconds

### MQTT not connecting

1. Verify MQTT broker is running and accessible
2. Check MQTT credentials in `config.h`
3. MQTT auto-reconnects every 30 seconds
4. Monitor serial output for error codes

### RTC not found

- System continues without scheduling if RTC fails after 5 attempts
- Check I2C wiring (SDA=GPIO5, SCL=GPIO4)
- Verify DS3231 module is powered correctly

### Valves not responding

- Check PCF8574 addresses (0x38, 0x3C)
- Verify I2C connections
- Test with manual MQTT valve_control command
- Check relay module power supply

### System hangs or restarts

- Hardware watchdog triggers after 30 seconds of inactivity
- Check serial monitor for watchdog messages
- Review recent code changes or MQTT commands

## Development

### Project Structure

```
esp32-irrigation/
├── include/              # Header files
│   ├── config.h         # User configuration (not in git)
│   ├── config.h.example # Configuration template
│   ├── lcd.h
│   ├── mqtt_commands.h
│   ├── mqtt_handler.h
│   ├── pump.h
│   ├── valves.h
│   ├── wifi_handler.h
│   └── utils.h
├── src/                 # Source files
│   ├── main.cpp
│   ├── lcd.cpp
│   ├── mqtt_commands.cpp
│   ├── mqtt_handler.cpp
│   ├── pump.cpp
│   ├── valves.cpp
│   ├── wifi_handler.cpp
│   └── utils.cpp
├── lib/
│   └── TaskManager/     # Custom task scheduling library
└── platformio.ini       # PlatformIO configuration
```

### Building

```bash
pio run              # Build only
pio run -t upload    # Build and upload
pio run -t clean     # Clean build files
pio device monitor   # Serial monitor
```

### Dependencies

- Arduino framework for ESP32
- RTClib (Adafruit)
- PubSubClient (MQTT)
- ArduinoJson
- LCD-I2C-HD44780
- PCF8574
- esp32FOTA

## Firmware Updates

The system supports OTA (Over-The-Air) updates via esp32FOTA:

1. Configure `FOTA_MANIFEST_URL` in `config.h`
2. System checks for updates every 10 minutes (when no tasks running)
3. Updates are downloaded and applied automatically

## Version History

### v0.0.3 (Current)
- Added MQTT command handling
- Implemented NVS configuration storage
- Added hardware watchdog timer
- Improved error recovery for RTC
- Fixed memory leaks and critical bugs
- Enhanced WiFi/MQTT reconnection logic

### v0.0.2
- Initial working version

## License

[Your License Here]

## Contributing

Contributions welcome! Please open an issue or pull request.

## Credits

- Inspired by [esp32-irrigation-automation](https://github.com/lrswss/esp32-irrigation-automation)
- Built with [PlatformIO](https://platformio.org/)
- Powered by [ESP32-C3](https://www.espressif.com/en/products/socs/esp32-c3)

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check the troubleshooting section above
- Review serial monitor output for diagnostic messages
