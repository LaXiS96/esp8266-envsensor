```sh
cd ~/esp
git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
cd ESP8266_RTOS_SDK
git switch release/v3.4
python tools/idf_tools.py install
python tools/idf_tools.py install-python-env

```

Use this exact version of export.ps1: https://github.com/espressif/esp-idf/blob/ccabacec32e18d65c8afc3571d6b65b70c3feadb/export.ps1

Use `ESP8266_RTOS_SDK/examples` for project setup.

TODO https://github.com/espressif/esp-idf-template/blob/master/main/CMakeLists.txt is different
