set(SOURCE_FILES_LIST
    "main.cpp"
    "Application/NetworkManager/NetworkManager.cpp"
    "Application/NetworkManager/NetworkInterface.cpp"
    "Application/NetworkManager/WiFiInterface.cpp"
    "Application/NetworkManager/DnsConfiguration.cpp"
    "Application/WebServerManager/WebServerManager.cpp"
    "Application/WebServerManager/StaticFileHandler.cpp"
    "Application/WebServerManager/WebSocketHandler.cpp"
    "Application/CommandManager/CommandManager.cpp"
    "Application/DeviceManager/DeviceManager.cpp"
    "Application/DeviceManager/DPS5020.cpp"
    "Application/MqttManager/MqttManager.cpp"
    "Application/UpdateManager/UpdateManager.cpp"
    "Application/SettingsManager/SettingsManager.cpp"
    "Application/LogManager/LogManager.cpp"
    "Application/TimeManager/TimeManager.cpp"
    "lib/communication/modbus/ModbusMaster.cpp"
    "lib/communication/modbus/ModbusPdu.cpp"
    "lib/communication/modbus/ModbusRtuClient.cpp"
    "lib/system/DateTime.cpp"
    "lib/system/TimeSpan.cpp"
)

set(INCLUDE_DIRS_LIST
    .
    "Application"
    "Application/NetworkManager"
    "Application/WebServerManager"
    "Application/CommandManager"
    "Application/DeviceManager"
    "Application/MqttManager"
    "Application/UpdateManager"
    "Application/SettingsManager"
    "Application/LogManager"
    "Application/TimeManager"
    "lib"
    "lib/common"
    "lib/communication/modbus"
    "lib/rtos"
    "lib/json"
    "lib/system"
)

set(COMPONENT_REQUIRES
    esp_timer
    esp_netif
    esp_event
    esp_wifi
    nvs_flash
    esp_http_server
    esp_app_format
    app_update
    esp_partition
    fatfs
    wear_levelling
    esp_driver_i2c
    esp_driver_gpio
    esp_driver_uart
    mdns
    espressif__mqtt
)

# Register component with ESP-IDF
idf_component_register(
    SRCS ${SOURCE_FILES_LIST}
    INCLUDE_DIRS ${INCLUDE_DIRS_LIST}
    REQUIRES ${COMPONENT_REQUIRES}
)
