#pragma once
#include "ServiceProvider.h"
#include "InitState.h"
#include "Task.h"
#include "modbus.h"
#include "DPS5020.h"

class DeviceManager
{
    inline constexpr static const char *TAG = "DeviceManager";

    // ESP32-C3 SuperMini: RX=GPIO20, TX=GPIO21
    static constexpr int UART_TX_PIN = 21;
    static constexpr int UART_RX_PIN = 20;
    static constexpr uint32_t UART_BAUD = 9600;
    static constexpr int POLL_INTERVAL_MS = 1000;

public:
    explicit DeviceManager(ServiceProvider &serviceProvider);

    DeviceManager(const DeviceManager &) = delete;
    DeviceManager &operator=(const DeviceManager &) = delete;

    void Init();

    DPS5020 &getDPS5020() { return dps5020_; }

private:
    ServiceProvider &serviceProvider_;
    InitState initState_;

    ModbusRtuClient rtuClient_;
    ModbusMaster master_{rtuClient_};
    DPS5020 dps5020_{master_, 1};

    Task pollTask_;

    void PollLoop();
};
