#include "DeviceManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DeviceManager::DeviceManager(ServiceProvider &ctx)
    : serviceProvider_(ctx)
{
}

void DeviceManager::Init()
{
    auto init = initState_.TryBeginInit();
    if (!init)
    {
        ESP_LOGW(TAG, "Already initialized or initializing");
        return;
    }

    led_.Init();

    rtuClient_.Init(BoardConfig::MODBUS_TX_PIN, BoardConfig::MODBUS_RX_PIN,
                    BoardConfig::MODBUS_BAUD);

    pollTask_.Init("dev_poll", 4, 4096);
    pollTask_.SetHandler([this]() { PollLoop(); });
    pollTask_.Run();

    init.SetReady();
    ESP_LOGI(TAG, "Initialized (TX=%d, RX=%d, baud=%lu)",
             BoardConfig::MODBUS_TX_PIN, BoardConfig::MODBUS_RX_PIN,
             (unsigned long)BoardConfig::MODBUS_BAUD);
}

void DeviceManager::PollLoop()
{
    // Give the DPS5020 a moment to be ready after power-on
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true)
    {
        dps5020_.Poll();

        if (dps5020_.IsOnline())
        {
            const auto &d = dps5020_.GetData();
            ESP_LOGD(TAG, "V=%.2fV I=%.2fA P=%.2fW Vin=%.2fV %s %s",
                     d.outVoltage, d.outCurrent, d.outPower, d.inVoltage,
                     d.outputOn ? "ON" : "OFF",
                     d.constantCurrent ? "CC" : "CV");
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
