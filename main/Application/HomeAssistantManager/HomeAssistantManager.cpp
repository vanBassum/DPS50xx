#include "HomeAssistantManager.h"
#include "MqttManager/MqttManager.h"
#include "DeviceManager/DeviceManager.h"
#include "DPS5020.h"
#include "JsonWriter.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char *const PROTECTION_LABELS[] = {"none", "OVP", "OCP", "OPP"};

HomeAssistantManager::HomeAssistantManager(ServiceProvider &ctx)
    : serviceProvider_(ctx)
{
}

void HomeAssistantManager::Init()
{
    auto init = initState_.TryBeginInit();
    if (!init)
    {
        ESP_LOGW(TAG, "Already initialized or initializing");
        return;
    }

    auto &mqtt = serviceProvider_.getMqttManager();

    // ── LED light entity ─────────────────────────────────────

    mqtt.RegisterCommand("led", [this](const char *data, int len)
    {
        bool on = (len >= 2 && strncmp(data, "ON", 2) == 0);
        serviceProvider_.getDeviceManager().getLed().Set(on);
        PublishLedState();
    });

    // ── DPS5020 commands ─────────────────────────────────────

    mqtt.RegisterCommand("voltage", [this](const char *data, int len)
    {
        char val[32] = {};
        int n = len < (int)sizeof(val) - 1 ? len : (int)sizeof(val) - 1;
        memcpy(val, data, n);
        serviceProvider_.getDeviceManager().getDPS5020().SetVoltage(static_cast<float>(atof(val)));
    });

    mqtt.RegisterCommand("current", [this](const char *data, int len)
    {
        char val[32] = {};
        int n = len < (int)sizeof(val) - 1 ? len : (int)sizeof(val) - 1;
        memcpy(val, data, n);
        serviceProvider_.getDeviceManager().getDPS5020().SetCurrent(static_cast<float>(atof(val)));
    });

    mqtt.RegisterCommand("output", [this](const char *data, int len)
    {
        bool on = (len >= 2 && (strncmp(data, "ON", 2) == 0 || strncmp(data, "on", 2) == 0))
                  || (len >= 1 && data[0] == '1');
        serviceProvider_.getDeviceManager().getDPS5020().SetOutput(on);
    });

    mqtt.RegisterCommand("keylock", [this](const char *data, int len)
    {
        bool on = (len >= 2 && (strncmp(data, "ON", 2) == 0 || strncmp(data, "on", 2) == 0))
                  || (len >= 1 && data[0] == '1');
        serviceProvider_.getDeviceManager().getDPS5020().SetKeyLock(on);
    });

    // ── HA Discovery ─────────────────────────────────────────

    mqtt.RegisterDiscovery([this]()
    {
        auto &mqtt = serviceProvider_.getMqttManager();

        // LED light
        mqtt.PublishEntityDiscovery("light", "led", [&mqtt](JsonWriter &json)
        {
            json.field("name", "LED");

            char topic[128];
            snprintf(topic, sizeof(topic), "%s/set/led", mqtt.GetBaseTopic());
            json.field("cmd_t", topic);

            snprintf(topic, sizeof(topic), "%s/led/state", mqtt.GetBaseTopic());
            json.field("stat_t", topic);

            json.field("pl_on", "ON");
            json.field("pl_off", "OFF");
        });

        // DPS5020 output switch
        mqtt.PublishEntityDiscovery("switch", "output", [&mqtt](JsonWriter &json)
        {
            json.field("name", "Output");
            json.field("ic", "mdi:power");

            char topic[128];
            snprintf(topic, sizeof(topic), "%s/set/output", mqtt.GetBaseTopic());
            json.field("cmd_t", topic);

            snprintf(topic, sizeof(topic), "%s/dps/outputOn", mqtt.GetBaseTopic());
            json.field("stat_t", topic);

            json.field("pl_on", "ON");
            json.field("pl_off", "OFF");
        });

        // DPS5020 sensors
        struct SensorDef
        {
            const char *objectId;
            const char *name;
            const char *subtopic;
            const char *deviceClass;
            const char *unit;
            const char *icon;
        };

        static const SensorDef dpsSensors[] = {
            {"out_voltage", "Output Voltage", "dps/outVoltage", "voltage",  "V", nullptr},
            {"out_current", "Output Current", "dps/outCurrent", "current",  "A", nullptr},
            {"out_power",   "Output Power",   "dps/outPower",   "power",    "W", nullptr},
            {"in_voltage",  "Input Voltage",  "dps/inVoltage",  "voltage",  "V", nullptr},
            {"set_voltage", "Set Voltage",    "dps/setVoltage", "voltage",  "V", "mdi:target"},
            {"set_current", "Set Current",    "dps/setCurrent", "current",  "A", "mdi:target"},
            {"protection",  "Protection",     "dps/protection", nullptr,    nullptr, "mdi:shield-alert"},
            {"cvcc",        "CV/CC Mode",     "dps/cvcc",       nullptr,    nullptr, "mdi:sine-wave"},
        };

        for (const auto &s : dpsSensors)
        {
            const char *oid = s.objectId;
            const char *sub = s.subtopic;

            mqtt.PublishEntityDiscovery("sensor", oid, [&mqtt, &s, sub](JsonWriter &json)
            {
                json.field("name", s.name);

                char topic[128];
                snprintf(topic, sizeof(topic), "%s/%s", mqtt.GetBaseTopic(), sub);
                json.field("stat_t", topic);

                if (s.deviceClass)
                    json.field("dev_cla", s.deviceClass);
                if (s.unit)
                    json.field("unit_of_meas", s.unit);
                if (s.icon)
                    json.field("ic", s.icon);
            });
        }

        PublishLedState();
        PublishDpsState();
    });

    // ── Periodic DPS5020 state publishing ────────────────────

    publishTimer_.Init("ha_dps", pdMS_TO_TICKS(1000), true);
    publishTimer_.SetHandler([this]()
    {
        if (serviceProvider_.getMqttManager().IsConnected())
            PublishDpsState();
    });
    publishTimer_.Start();

    init.SetReady();
    ESP_LOGI(TAG, "Initialized");
}

void HomeAssistantManager::PublishLedState()
{
    bool on = serviceProvider_.getDeviceManager().getLed().IsOn();
    serviceProvider_.getMqttManager().Publish("led/state", on ? "ON" : "OFF", true);
}

void HomeAssistantManager::PublishDpsState()
{
    auto &mqtt = serviceProvider_.getMqttManager();
    auto &dps = serviceProvider_.getDeviceManager().getDPS5020();
    const auto &d = dps.GetData();

    mqtt.Publish("dps/online", dps.IsOnline() ? "true" : "false", true);

    char buf[16];

    snprintf(buf, sizeof(buf), "%.2f", d.setVoltage);
    mqtt.Publish("dps/setVoltage", buf, true);

    snprintf(buf, sizeof(buf), "%.2f", d.setCurrent);
    mqtt.Publish("dps/setCurrent", buf, true);

    snprintf(buf, sizeof(buf), "%.2f", d.outVoltage);
    mqtt.Publish("dps/outVoltage", buf, true);

    snprintf(buf, sizeof(buf), "%.2f", d.outCurrent);
    mqtt.Publish("dps/outCurrent", buf, true);

    snprintf(buf, sizeof(buf), "%.2f", d.outPower);
    mqtt.Publish("dps/outPower", buf, true);

    snprintf(buf, sizeof(buf), "%.2f", d.inVoltage);
    mqtt.Publish("dps/inVoltage", buf, true);

    mqtt.Publish("dps/outputOn", d.outputOn ? "ON" : "OFF", true);
    mqtt.Publish("dps/protection", PROTECTION_LABELS[static_cast<int>(d.protection) & 3], true);
    mqtt.Publish("dps/cvcc", d.constantCurrent ? "CC" : "CV", true);
    mqtt.Publish("dps/keyLock", d.keyLock ? "on" : "off", true);
}
