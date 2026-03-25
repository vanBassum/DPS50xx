#pragma once

#include "ServiceProvider.h"
#include "InitState.h"
#include "Task.h"
#include "mqtt_client.h"

class MqttManager
{
    inline constexpr static const char *TAG = "MqttManager";

    static constexpr int PUBLISH_INTERVAL_MS = 1000;

public:
    explicit MqttManager(ServiceProvider &serviceProvider);

    MqttManager(const MqttManager &) = delete;
    MqttManager &operator=(const MqttManager &) = delete;

    void Init();
    bool IsConnected() const { return connected_; }

private:
    ServiceProvider &serviceProvider_;
    InitState initState_;

    esp_mqtt_client_handle_t client_ = nullptr;
    bool connected_ = false;
    bool enabled_ = false;
    char prefix_[64] = {};

    Task publishTask_;

    void StartClient();
    void PublishLoop();
    void PublishState();
    void Subscribe();
    void HandleCommand(const char *topic, const char *data, int dataLen);

    // Build full topic: "{prefix}/{suffix}"
    int BuildTopic(char *buf, size_t bufSize, const char *suffix) const;

    static void EventHandlerStatic(void *arg, esp_event_base_t base, int32_t id, void *data);
    void HandleEvent(esp_mqtt_event_handle_t event);
};
