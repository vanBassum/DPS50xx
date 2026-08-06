#include "BoardContext.h"
#include "driver/uart.h"
#include "esp_log.h"

void BoardContext::Init()
{
    auto initAttempt = initState_.TryBeginInit();
    if (!initAttempt)
    {
        ESP_LOGW(TAG, "Already initialized or initializing");
        return;
    }

    led_.Init();

    // The UART host for the Modbus master. Brought up here rather than by the
    // manager that polls the supply: the bus is the board's, and a second
    // device on the same wire would share it.
    rtu_.Init(BoardConfig::MODBUS_TX_PIN,
              BoardConfig::MODBUS_RX_PIN,
              BoardConfig::MODBUS_BAUD,
              static_cast<uart_port_t>(BoardConfig::MODBUS_UART_PORT));

    initAttempt.SetReady();
    ESP_LOGI(TAG, "Initialized (LED=%d, Modbus TX=%d RX=%d @ %lu baud, unit %u)",
             BoardConfig::LED_PIN,
             BoardConfig::MODBUS_TX_PIN,
             BoardConfig::MODBUS_RX_PIN,
             static_cast<unsigned long>(BoardConfig::MODBUS_BAUD),
             static_cast<unsigned>(BoardConfig::DPS_UNIT_ADDRESS));
}
