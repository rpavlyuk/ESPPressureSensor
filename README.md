[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
[![Made in Ukraine](https://img.shields.io/badge/Made_in-Ukraine-ffd700.svg?labelColor=0057b7)](https://stand-with-ukraine.pp.ua)

# ESP32 Pressure Sensor Firmware

## Purpose
This project provides firmware for an ESP32-based pressure sensor system using the DFRobot SEN0257 pressure sensor. The firmware is designed to read pressure data from the sensor and support integration with smart home platforms.

### Features
- **WiFi Connectivity**: Supports connecting to WiFi for remote monitoring.
- **MQTT Support**: Publishes sensor data via MQTT for integration with various platforms.
- **Home Assistant Integration**: Automatically detects and configures the sensor in Home Assistant.
- **Web Interface**: Provides a web-based user interface for configuration and monitoring.
- **Zigbee (Experimental)**: Zigbee functionality is present but currently disabled by default.

## Prerequisites
To get started, you will need:
- **Hardware**:
  - ESP32-C6 or ESP32-S3 microcontroller (both have been tested).
  - DFRobot SEN0257 pressure sensor.
- **Software**:
  - ESP-IDF framework (version 4.4 or higher recommended).

## Wiring
The following wiring schema is expected by default:
```
+------------+-----------+
| Sensor Pin | ESP32 Pin |
+------------+-----------+
| GND        | GND       |
+------------+-----------+
| VCC        | +5V       |
+------------+-----------+
| Signal     | IO03      |
+------------+-----------+
```
It is very recommended to connect a 0.1uF ceramic capacitor between `GND` and `IO03` to act as the filter and suppress voltage spikes and drops.

You may consider the other IO pin so you can change `PRESSURE_SENSOR_PIN` in file `sensor.h`. But keep in mind that not all ESP32 variants have ADC feature enabled and it may differ between ESP32 variants.

## Building and Flashing
1. **Setup the ESP-IDF Environment**:
   - Follow the official [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to configure your development environment.
   - Further on we will assume, that you're using Unix-based OS (Linux, macos) and you've installed ESP-IDF into `$HOME/esp-idf` folder.
2. **Clone this Repository**:
   ```bash
   git clone https://github.com/rpavlyuk/ESPPressureSensor
   cd ESPPressureSensor
   ```
3. **Initiate ESP-IDF**:
  This will map the source folder to ESP-IDF framework and will enable all needed global variables.
   ```bash
   . $HOME/esp-idf/export.sh
   ```
4. **Build the firmware**:
   ```bash
   idf.py build
   ```
   You will get `idf.py: command not found` if you didn't initiate ESP-IDF in the previous step.
   You may also get build errors if you've selected other board type then `ESP32-C6` or `ESP32-S3`.
5. **Determine the port**:
  A connected ESP32 device is opening USB-2-Serial interface, which your system might see as `/dev/tty.usbmodem1101` (macos example) or `/dev/ttyUSB0` (Linux example). Use `ls -al /dev` command to see the exact one.
6. **Flash the Firmware**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   Replace `/dev/ttyUSB0` with the appropriate port for your system.

> [!NOTE]
> Every time you change the board type (e.g., from ESP32-C6 to ESP32-S3, the framework is re-creating `sdkconfig` file which makes some very important settings gone. Thus, you (might) need to change/set some settings **if you've changed the device board**:
* Open SDK settings menu by running:
  ```bash
  idf.py menuconfig
  ```
* Set the following parameters:
  * *Partition table -> Custom partition table CSV* as the option
  * *Partition table -> Custom partition CSV file* set to `partition_table/partition.csv`
  * *Serial flasher config -> Flash size* to 4Mb or 8Mb
  * *Serial flasher config -> Detect flash size when flashing bootloader* to enabled
  * *Component config -> HTTP Server -> Max HTTP Request Header Length* to `8192`
  * *Component config → ESP System Settings -> Event loop task stack size* to `4096`
  * *Component config → ESP System Settings -> Main task stack size* to `4096`
  * *Component config → ESP System Settings -> Minimal allowed size for shared stack* to `2048`


## Initiation
### WiFi Setup
* On the first boot, the device will start in access point mode with the SSID `PROV_AP_XXXXXX`. The exact named will be different depending on the device hardware ID (which is built in). The password is SSID name plus `1234`. For example, `PROV_AP_XXXXXX1234`
* Use the *ESP SoftAP Prov* ([iOS](https://apps.apple.com/us/app/esp-softap-provisioning/id1474040630), [Android](https://play.google.com/store/apps/details?id=com.espressif.provsoftap&hl=en)) mobile app to connect to the device and configure WiFi settings. Read more [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html#provisioning-tools) if you want to know more about the SoftAP provisioning.

### Device Setup
* Device will initiate itself with default settings once the WiFi was provisioned. All further configuration, including **sensor calibration**, will/can be made via WEB interface.
> [!NOTE]
> Currently only DHCP mode is supported.

## WEB Setup
* Device starts the WEB interfance available as `http://<WIFI-IP>/`.
* You can start seeing the pressure readings by visiting **Status** page.
* Basic configuration parameters on **Configuration** page:
  * `MQTT Mode`: defines how MQTT works
    - `Disabled`: MQTT is disabled
    - `Connect Once`: MQTT is enabled. Connection will be established, but no reconnect attempts will be made once it is dropped.
    - `Auto-Connect`: MQTT is enabled. Connection will operate in keep-alive mode, reconnecting on failures.
  * `MQTT Server`, `MQTT Port`, `MQTT Protocol`, `MQTT User`, `MQTT Password`: MQTT connection string parameters. Protocol `mqtts` is supported but CA/root certificate management UI has not been implemented yet.
  * `MQTT Prefix`: top level path in the MQTT tree. The path will look like: `<MQTT_prefix>/<device_id>/...`
  * `HomeAssistant Device integration MQTT Prefix`: HomeAssistant MQTT device auto-discovery prefix. Usually, it is set to `homeassistant`
  * `HomeAssistant Device update interval (ms)`: how often to update device definitions at HomeAssistant.
* Sensor parameters:
  * `Sensing interval (ms)`: how often to read the data from sensor
  * `Sensor ADC Offset (V)`: calibration parameter. It represents which voltage corresponds to a zero pressure. We will explain calibration in separate section.
  * `Sensor Linear Multiplier`: this is a linear multiplier (dependency) between voltage in Volts and pressure in Pascals. No need to change it unless you know why.
  * `Number of samples to collect per measurement`, `Interval between samples (ms)`, `Threshold for samples filtering (%)`: these are advanced measurement sampling parameters. The device implements smart measurement when collects N samples of voltage (ADC) per one measurement with certain small interval, calculates the mediane and drops all other then deviate from median by certain threshold.

## Calibration
1. Connect the pressure sensor to ESP32 device and leave it open. Means, do not mount it into the tank or pipe.
2. Go to the WEB interface, open **Status** page and note the `Voltage` value. For example, it can something like `0.489 V`
3. Go to the **Config** tab, set this value as `Sensor ADC Offset (V)` parameter and reboot the device.

You may into more advanced mode and do more precise calibration if you analon pressure manometer. In this case you may adjust also `Sensor Linear Multiplier`.

## Home Assistant Integration
The device will automatically enable itself in Home Assistant if:
* both HA and the device are connected to the same MQTT server
* `HomeAssistant Device integration MQTT Prefix` is set correctly
* auto-discovery is enable in Home Assistant (should be enabled by default)

## Known issues, problems and TODOs:
* CA certification configuration for SSL (mqtts) mode to be implemented
* Static IP support needed
* Device may have memory leaks when used very intensively (to be improved)


## License and Credits
* GPLv3 -- you're free to use and modify the code
* Uses [ESP32_NVS](https://github.com/VPavlusha/ESP32_NVS) library by [VPavlusha](https://github.com/VPavlusha)
* Consider putting a star if you like the project
* Find me at roman.pavlyuk@gmail.com
