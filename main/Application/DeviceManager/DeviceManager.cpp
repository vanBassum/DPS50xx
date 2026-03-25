#include "DeviceManager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DeviceManager::DeviceManager(ServiceProvider &serviceProvider)
    : serviceProvider_(serviceProvider)
{
}

void DeviceManager::Init()
{
    initState_.Begin();

    rtuClient_.Init(UART_TX_PIN, UART_RX_PIN, UART_BAUD);

    pollTask_.Run("dev_poll", 4, 4096, [this]()
    {
        PollLoop();
    });

    ESP_LOGI(TAG, "DeviceManager initialized (TX=%d, RX=%d, baud=%lu)",
             UART_TX_PIN, UART_RX_PIN, (unsigned long)UART_BAUD);

    initState_.Complete();
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
