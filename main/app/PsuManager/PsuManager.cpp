#include "PsuManager.h"
#include "BoardContext.h"
#include "StruxProvider.h"
#include "SettingsManager.h"
#include "CommandManager.h"
#include "TelemetryManager.h"
#include "ModbusError.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

PsuManager::PsuManager(AppProvider& app)
    : app_(app)
{
}

void PsuManager::Init()
{
    auto initAttempt = initState_.TryBeginInit();
    if (!initAttempt)
    {
        ESP_LOGW(TAG, "Already initialized or initializing");
        return;
    }

    // Reaching DOWN into the framework, which is the only direction allowed.
    // Nothing in strux/ was edited to make these two lines work.
    StruxProvider& strux = app_.getStrux();
    strux.getSettingsManager().Register({ &pollIntervalMs_, &telemetry_ });
    strux.getCommandManager().Register(this, commands_);

    // The board already brought the UART host up — the bus is the board's, not
    // this manager's. All that is left is to start asking.
    pollTask_.Init("psu_poll", TASK_PRIORITY, TASK_STACK);
    pollTask_.SetHandler([this] { PollLoop(); });
    pollTask_.Run();

    initAttempt.SetReady();
    ESP_LOGI(TAG, "Initialized");
}

void PsuManager::PollLoop()
{
    vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));

    DPS5020& dps = app_.getBoard().GetDps();

    while (true)
    {
        dps.Poll();

        const bool online = dps.IsOnline();
        if (online != lastOnline_)
        {
            ShowLinkState(online);
            lastOnline_ = online;
        }

        if (online)
        {
            const DPS5020Data& d = dps.GetData();
            ESP_LOGD(TAG, "V=%.2fV I=%.2fA P=%.2fW Vin=%.2fV %s %s",
                     d.outVoltage, d.outCurrent, d.outPower, d.inVoltage,
                     d.outputOn ? "ON" : "OFF",
                     d.constantCurrent ? "CC" : "CV");

            if (telemetry_.Get())
                Record(d);
        }

        // Re-read every iteration so a period changed in the settings UI takes
        // effect on the next poll. Get() is a field read, not an NVS read.
        vTaskDelay(pdMS_TO_TICKS(pollIntervalMs_.Get()));
    }
}

void PsuManager::Record(const DPS5020Data& d)
{
    auto point = app_.getStrux().getTelemetryManager().Measure("psu");
    point.Tag("mode", d.constantCurrent ? "cc" : "cv");
    point.Field("voltage", d.outVoltage);
    point.Field("current", d.outCurrent);
    point.Field("power", d.outPower);
    point.Field("inputVoltage", d.inVoltage);
    point.Field("setVoltage", d.setVoltage);
    point.Field("setCurrent", d.setCurrent);
    point.Field("output", d.outputOn);
    point.Commit();
}

void PsuManager::ShowLinkState(bool online)
{
    app_.getBoard().GetLed().Set(online);
}

// ──────────────────────────────────────────────────────────────
// Commands. These appear in `help list` and work over the local WebSocket and
// the relay alike, because a handler serves neither — it serves a
// CommandContext.
// ──────────────────────────────────────────────────────────────

RequestError PsuManager::Cmd_Get(CommandContext& ctx)
{
    RETURN_IF_ERROR(ctx.readArgs());

    DPS5020& dps = app_.getBoard().GetDps();
    const DPS5020Data& d = dps.GetData();

    auto resp = ctx.reply.object();
    resp.field("online", dps.IsOnline());
    resp.field("setVoltage", d.setVoltage);
    resp.field("setCurrent", d.setCurrent);
    resp.field("outVoltage", d.outVoltage);
    resp.field("outCurrent", d.outCurrent);
    resp.field("outPower", d.outPower);
    resp.field("inVoltage", d.inVoltage);
    resp.field("keyLock", d.keyLock);
    resp.field("protection", static_cast<int32_t>(d.protection));
    resp.field("constantCurrent", d.constantCurrent);
    resp.field("outputOn", d.outputOn);
    resp.field("backlight", static_cast<int32_t>(d.backlight));
    resp.field("model", static_cast<int32_t>(d.model));
    resp.field("version", static_cast<int32_t>(d.version));
    return RequestError::Ok;
}

RequestError PsuManager::Cmd_Set(CommandContext& ctx)
{
    DPS5020& dps = app_.getBoard().GetDps();
    const DPS5020Data& d = dps.GetData();

    // Every destination starts at the supply's current value, so an argument the
    // caller omitted compares equal below and writes nothing. That is what makes
    // one `psu set` able to carry any subset of the setpoints without five
    // Modbus round-trips for the four things that did not change — and it
    // replaces the old field/value string pair, which could only ever carry one.
    float    voltage   = d.setVoltage;
    float    current   = d.setCurrent;
    bool     output    = d.outputOn;
    bool     keyLock   = d.keyLock;
    uint32_t backlight = d.backlight;

    RETURN_IF_ERROR(ctx.readArgs(
        Optional("voltage",   voltage),
        Optional("current",   current),
        Optional("output",    output),
        Optional("keyLock",   keyLock),
        Optional("backlight", backlight)
    ));

    // MEANING validation, which is the handler's half of the contract: the
    // framework has already established these are numbers. Refuse rather than
    // clamp — the supply would silently accept a truncated register and the
    // caller would never learn its 500 became 50.
    auto refuse = [&ctx](const char* why) {
        auto resp = ctx.reply.object();
        resp.field("ok", false);
        resp.field("error", why);
        return RequestError::Ok;
    };

    if (voltage < 0.0f || voltage > MAX_VOLTAGE)
        return refuse("voltage out of range (0-50 V)");
    if (current < 0.0f || current > MAX_CURRENT)
        return refuse("current out of range (0-20 A)");
    if (backlight > MAX_BACKLIGHT)
        return refuse("backlight out of range (0-5)");

    // Apply in an order that cannot brown-out a load: the limits move before the
    // output is switched on, and the output goes off before anything else when
    // that is what was asked. Key lock last, because locking the panel while a
    // write is still in flight is how the supply refuses the rest.
    ModbusError err = ModbusError::NoError;
    const bool turningOff = (output != d.outputOn) && !output;

    if (err == ModbusError::NoError && turningOff)
        err = dps.SetOutput(false);

    if (err == ModbusError::NoError && voltage != d.setVoltage)
        err = dps.SetVoltage(voltage);
    if (err == ModbusError::NoError && current != d.setCurrent)
        err = dps.SetCurrent(current);

    if (err == ModbusError::NoError && !turningOff && output != d.outputOn)
        err = dps.SetOutput(true);

    if (err == ModbusError::NoError && backlight != d.backlight)
        err = dps.SetBacklight(static_cast<uint8_t>(backlight));
    if (err == ModbusError::NoError && keyLock != d.keyLock)
        err = dps.SetKeyLock(keyLock);

    // A Modbus failure is meaning, not form: it goes in the reply where it can
    // say which error it was, rather than becoming a framework REJECT.
    auto resp = ctx.reply.object();
    resp.field("ok", err == ModbusError::NoError);
    if (err != ModbusError::NoError)
        resp.field("error", ModbusErrorToString(err));

    // Echo what the supply now holds, so a caller needs no follow-up `psu get`.
    resp.field("setVoltage", d.setVoltage);
    resp.field("setCurrent", d.setCurrent);
    resp.field("outputOn", d.outputOn);
    resp.field("keyLock", d.keyLock);
    resp.field("backlight", static_cast<int32_t>(d.backlight));
    return RequestError::Ok;
}
