#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Timer.h"

class HomeAssistantManager
{
    static constexpr const char *TAG = "HomeAssistantManager";

public:
    explicit HomeAssistantManager(ServiceProvider &serviceProvider);

    HomeAssistantManager(const HomeAssistantManager &) = delete;
    HomeAssistantManager &operator=(const HomeAssistantManager &) = delete;
    HomeAssistantManager(HomeAssistantManager &&) = delete;
    HomeAssistantManager &operator=(HomeAssistantManager &&) = delete;

    void Init();

private:
    ServiceProvider &serviceProvider_;
    InitState initState_;
    Timer publishTimer_;

    void PublishLedState();
    void PublishDpsState();
};
