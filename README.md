# esp8266-envsensor

```sh
# Install SDK:
cd ~/esp
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
cd ESP8266_RTOS_SDK
git switch release/v3.4
python tools/idf_tools.py install
python tools/idf_tools.py install-python-env

# Put this exact version of export.ps1 in ~/esp/ESP8266_RTOS_SDK:
# https://github.com/espressif/esp-idf/blob/ccabacec32e18d65c8afc3571d6b65b70c3feadb/export.ps1

# Use SDK:
~/esp/ESP8266_RTOS_SDK/export.ps1
idf.py build
idf.py -p COM3 flash monitor
```

https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/index.html
https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

## Low power consumption

- GPIO16 (NodeMCU D0) **must** be connected to RST for wakeup to work
    - https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/api-reference/system/sleep_modes.html
- Insecure/open WiFi network and static BSSID, channel and IP configuration ensure fastest WiFi connection
    - https://blog.voneicken.com/2018/lp-wifi-esp8266-1/
    - Enable MAC filtering on router, to block random people connecting to open WiFi network
- WiFi must be stopped before sleeping
    - Disabled RF calibration on wakeup
- 80MHz is the lowest clock frequency available
- TODO lower wifi tx power
- TODO hi-z gpios?

https://panther.kapsi.fi/posts/2019-05-14_esp8266_deep_sleep

### Measurements

- 1 Ohm shunt resistor: 1mV = 1mA
- power ESP8266 from battery or isolate oscilloscope (do not share grounds, use laptop on battery for USB oscilloscope)
- oscilloscope probe across shunt resistor

TODO add screenshots

## ESP-12 white breakout board

**Silkscreen labels are for through holes, not SMD pads!**

| Pin | On board | OK? |
|-----|----------|-----|
| RST | - | needs pullup
| CH_PD | 10k to VCC | OK
| GPIO15 | 10k to GND | OK
| GPIO0 | - | needs pullup for normal boot, hold low during reset for flashing
| GPIO2 | - | OK (internal pullup)

https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/boot-mode-selection.html
