#include "MqttManager.h"
#include "DeviceManager.h"
#include "SettingsManager.h"
#include "JsonWriter.h"
#include "BufferStream.h"
#include "DPS5020.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

MqttManager::MqttManager(ServiceProvider &serviceProvider)
    : serviceProvider_(serviceProvider)
{
}

void MqttManager::Init()
{
    auto initAttempt = initState_.TryBeginInit();
    if (!initAttempt)
    {
        ESP_LOGW(TAG, "Already initialized or initializing");
        return;
    }

    auto &settings = serviceProvider_.getSettingsManager();

    char enabledStr[8] = {};
    settings.getBool("mqtt.enabled", &enabled_);
    if (!enabled_)
    {
        ESP_LOGI(TAG, "MQTT disabled");
        initAttempt.SetReady();
        return;
    }

    settings.getString("mqtt.prefix", prefix_, sizeof(prefix_));
    if (prefix_[0] == '\0')
        strncpy(prefix_, "dps50xx", sizeof(prefix_));

    StartClient();

    publishTask_.Init("mqtt_pub", 4, 4096);
    publishTask_.SetHandler([this]() { PublishLoop(); });
    publishTask_.Run();

    ESP_LOGI(TAG, "MQTT initialized (prefix=%s)", prefix_);
    initAttempt.SetReady();
}

void MqttManager::StartClient()
{
    auto &settings = serviceProvider_.getSettingsManager();

    char broker[128] = {};
    char user[64] = {};
    char pass[64] = {};
    int32_t port = 1883;

    settings.getString("mqtt.broker", broker, sizeof(broker));
    settings.getString("mqtt.user", user, sizeof(user));
    settings.getString("mqtt.pass", pass, sizeof(pass));
    settings.getInt("mqtt.port", &port);

    if (broker[0] == '\0')
    {
        ESP_LOGW(TAG, "No MQTT broker configured");
        return;
    }

    // Build URI: mqtt://broker:port
    char uri[192] = {};
    snprintf(uri, sizeof(uri), "mqtt://%s:%ld", broker, (long)port);

    // LWT topic
    char lwtTopic[96] = {};
    BuildTopic(lwtTopic, sizeof(lwtTopic), "available");

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri;
    cfg.credentials.username = user[0] ? user : nullptr;
    cfg.credentials.authentication.password = pass[0] ? pass : nullptr;
    cfg.session.last_will.topic = lwtTopic;
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.msg_len = 7;
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = 1;

    client_ = esp_mqtt_client_init(&cfg);
    if (!client_)
    {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, EventHandlerStatic, this);
    esp_mqtt_client_start(client_);

    ESP_LOGI(TAG, "MQTT client started: %s", uri);
}

int MqttManager::BuildTopic(char *buf, size_t bufSize, const char *suffix) const
{
    return snprintf(buf, bufSize, "%s/%s", prefix_, suffix);
}

// ── Event handling ──────────────────────────────────────────

void MqttManager::EventHandlerStatic(void *arg, esp_event_base_t, int32_t id, void *data)
{
    auto *self = static_cast<MqttManager *>(arg);
    self->HandleEvent(static_cast<esp_mqtt_event_handle_t>(data));
}

void MqttManager::HandleEvent(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        connected_ = true;
        Subscribe();
        {
            char topic[96];
            BuildTopic(topic, sizeof(topic), "available");
            esp_mqtt_client_publish(client_, topic, "online", 6, 1, 1);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        connected_ = false;
        break;

    case MQTT_EVENT_DATA:
        HandleCommand(event->topic, event->data, event->data_len);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

void MqttManager::Subscribe()
{
    char topic[96];
    BuildTopic(topic, sizeof(topic), "set/#");
    esp_mqtt_client_subscribe(client_, topic, 1);
    ESP_LOGI(TAG, "Subscribed to %s", topic);
}

void MqttManager::HandleCommand(const char *topic, const char *data, int dataLen)
{
    if (!topic || !data || dataLen <= 0)
        return;

    // Extract suffix after "{prefix}/set/"
    char setPrefix[96];
    BuildTopic(setPrefix, sizeof(setPrefix), "set/");
    size_t prefixLen = strlen(setPrefix);

    if (strncmp(topic, setPrefix, prefixLen) != 0)
        return;

    const char *cmd = topic + prefixLen;

    // Null-terminate data
    char val[32] = {};
    int len = dataLen < (int)sizeof(val) - 1 ? dataLen : (int)sizeof(val) - 1;
    memcpy(val, data, len);
    val[len] = '\0';

    auto &dps = serviceProvider_.getDeviceManager().getDPS5020();
    ModbusError err = ModbusError::InvalidArguments;

    if (strcmp(cmd, "voltage") == 0)
        err = dps.SetVoltage(static_cast<float>(atof(val)));
    else if (strcmp(cmd, "current") == 0)
        err = dps.SetCurrent(static_cast<float>(atof(val)));
    else if (strcmp(cmd, "output") == 0)
        err = dps.SetOutput(strcmp(val, "on") == 0 || strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
    else if (strcmp(cmd, "keylock") == 0)
        err = dps.SetKeyLock(strcmp(val, "on") == 0 || strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
    else
        ESP_LOGW(TAG, "Unknown MQTT command: %s", cmd);

    if (err != ModbusError::NoError && err != ModbusError::InvalidArguments)
        ESP_LOGW(TAG, "MQTT command '%s' failed: %s", cmd, ModbusErrorToString(err));
}

// ── Publishing ──────────────────────────────────────────────

void MqttManager::PublishLoop()
{
    vTaskDelay(pdMS_TO_TICKS(5000)); // wait for network + MQTT connect

    while (true)
    {
        if (connected_)
            PublishState();

        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    }
}

void MqttManager::PublishState()
{
    auto &dps = serviceProvider_.getDeviceManager().getDPS5020();
    const auto &d = dps.GetData();

    char buf[512];
    BufferStream stream(buf, sizeof(buf));
    JsonWriter json(stream);

    json.beginObject();
    json.field("online", dps.IsOnline());
    json.field("setVoltage", d.setVoltage);
    json.field("setCurrent", d.setCurrent);
    json.field("outVoltage", d.outVoltage);
    json.field("outCurrent", d.outCurrent);
    json.field("outPower", d.outPower);
    json.field("inVoltage", d.inVoltage);
    json.field("outputOn", d.outputOn);
    json.field("protection", static_cast<int32_t>(d.protection));
    json.field("cvcc", d.constantCurrent ? "CC" : "CV");
    json.field("keyLock", d.keyLock);
    json.endObject();

    char topic[96];
    BuildTopic(topic, sizeof(topic), "state");
    esp_mqtt_client_publish(client_, topic, buf, (int)stream.length(), 0, 1);
}
