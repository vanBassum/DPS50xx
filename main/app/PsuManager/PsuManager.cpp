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
#include <cstdio>

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

    Psu& psu = app_.getBoard().GetPsu();

    while (true)
    {
        psu.Poll();

        const bool online = psu.IsOnline();
        if (online != lastOnline_)
        {
            ShowLinkState(online);
            lastOnline_ = online;
        }

        if (online)
        {
            // The log line names the readings it prints, because a human reads
            // it and wants the same five values every time — this is the one
            // consumer a generic walk would make worse.
            ESP_LOGD(TAG, "V=%.2fV I=%.2fA P=%.2fW Vin=%.2fV %s %s",
                     psu.Number(PsuKey::OutVoltage), psu.Number(PsuKey::OutCurrent),
                     psu.Number(PsuKey::OutPower), psu.Number(PsuKey::InVoltage),
                     psu.Flag(PsuKey::OutputOn) ? "ON" : "OFF",
                     psu.Flag(PsuKey::ConstantCurrent) ? "CC" : "CV");

            if (telemetry_.Get())
                Record(psu);
        }

        // Re-read every iteration so a period changed in the settings UI takes
        // effect on the next poll. Get() is a field read, not an NVS read.
        vTaskDelay(pdMS_TO_TICKS(pollIntervalMs_.Get()));
    }
}

void PsuManager::Record(Psu& psu)
{
    auto point = app_.getStrux().getTelemetryManager().Measure("psu");

    // The mode is a TAG and not a field — it is what an Influx query groups
    // by — so it is the one thing here that is still named. Every field comes
    // off the chain under the telemetry name its driver gave it.
    point.Tag("mode", psu.Flag(PsuKey::ConstantCurrent) ? "cc" : "cv");

    for (const PsuReading& r : psu)
    {
        if (r.telemKey == nullptr)
            continue;
        if (r.kind == PsuKind::Bool)
            point.Field(r.telemKey, r.AsBool());
        else
            point.Field(r.telemKey, static_cast<double>(r.value));
    }

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

/// The wire name for a reading's kind. A converter, so it lives here at the
/// edge rather than on the role — same split as SettingsManager keeping its
/// JSON out of Setting.
static const char* KindName(PsuKind kind)
{
    switch (kind)
    {
    case PsuKind::Bool: return "bool";
    case PsuKind::Enum: return "enum";
    case PsuKind::Number:
    default:            return "number";
    }
}

RequestError PsuManager::Cmd_Get(CommandContext& ctx)
{
    RETURN_IF_ERROR(ctx.readArgs());

    Psu& psu = app_.getBoard().GetPsu();

    // The reply DESCRIBES itself: every reading carries its own label, unit,
    // kind and writable range, so the browser renders a supply it was never
    // told about and this handler names not one register.
    auto resp = ctx.reply.object();
    resp.field("online", psu.IsOnline());

    auto readings = resp.object("readings");
    for (const PsuReading& r : psu)
    {
        auto entry = readings.object(r.key);
        entry.field("label", r.label);
        entry.field("unit", r.unit);
        entry.field("kind", KindName(r.kind));
        entry.field("value", r.value);
        entry.field("access", r.Writable() ? "rw" : "r");

        // An enum's codes are named by the supply that defines them, which is
        // what let PROTECTION_LABELS die in the frontend.
        if (const char* label = r.ValueLabel())
            entry.field("valueLabel", label);

        // The bounds the browser draws its inputs from, and the same ones
        // `psu set` refuses against below.
        if (r.Writable())
        {
            entry.field("min", r.min);
            entry.field("max", r.max);
        }
    }
    return RequestError::Ok;
}

RequestError PsuManager::Cmd_Set(CommandContext& ctx)
{
    Psu& psu = app_.getBoard().GetPsu();

    // Every destination starts at the supply's current value, so an argument the
    // caller omitted compares equal below and writes nothing. That is what makes
    // one `psu set` able to carry any subset of the setpoints without five
    // Modbus round-trips for the four things that did not change — and it
    // replaces the old field/value string pair, which could only ever carry one.
    float    voltage   = psu.Number(PsuKey::SetVoltage);
    float    current   = psu.Number(PsuKey::SetCurrent);
    bool     output    = psu.Flag(PsuKey::OutputOn);
    bool     keyLock   = psu.Flag(PsuKey::KeyLock);
    uint32_t backlight = static_cast<uint32_t>(psu.Number(PsuKey::Backlight));

    // The argument names are the supply-independent ones: a caller says
    // `-voltage`, and which register that is remains the driver's business.
    RETURN_IF_ERROR(ctx.readArgs(
        Optional("voltage",   voltage),
        Optional("current",   current),
        Optional("output",    output),
        Optional("keyLock",   keyLock),
        Optional("backlight", backlight)
    ));

    auto refuse = [&ctx](const char* why) {
        auto resp = ctx.reply.object();
        resp.field("ok", false);
        resp.field("error", why);
        return RequestError::Ok;
    };

    // MEANING validation, which is the handler's half of the contract: the
    // framework has already established these are numbers. Refuse rather than
    // clamp — the supply would silently accept a truncated register and the
    // caller would never learn its 500 became 50.
    //
    // The RANGE is now the reading's own, so this validates an XY6020L against
    // 60 V without knowing that is what it is talking to. A key the supply does
    // not have is refused rather than ignored, which is how `-backlight` fails
    // on a supply with no display to light.
    //
    // Only what the caller actually CHANGED is checked. An omitted argument
    // still holds the supply's own value, and the supply's own value can sit
    // outside these limits — the front panel set 20.1 A on the unit this was
    // first tested against. Validating those too meant one `psu set -voltage 6`
    // came back "current out of range" about a current the caller never
    // mentioned and could not fix through this command.
    const bool setV = voltage   != psu.Number(PsuKey::SetVoltage);
    const bool setI = current   != psu.Number(PsuKey::SetCurrent);
    const bool setB = backlight != static_cast<uint32_t>(psu.Number(PsuKey::Backlight));
    const bool setO = output    != psu.Flag(PsuKey::OutputOn);
    const bool setL = keyLock   != psu.Flag(PsuKey::KeyLock);

    char why[96] = {};
    auto valid = [&](const char* key, float value, bool changed) {
        if (!changed)
            return true;
        const PsuReading* r = psu.Find(key);
        if (r == nullptr)
        {
            snprintf(why, sizeof(why), "this supply has no '%s'", key);
            return false;
        }
        if (!r->InRange(value))
        {
            snprintf(why, sizeof(why), "%s out of range (%g-%g%s%s)",
                     r->label, r->min, r->max, r->unit[0] ? " " : "", r->unit);
            return false;
        }
        return true;
    };

    if (!valid(PsuKey::SetVoltage, voltage, setV)) return refuse(why);
    if (!valid(PsuKey::SetCurrent, current, setI)) return refuse(why);
    if (!valid(PsuKey::Backlight, static_cast<float>(backlight), setB)) return refuse(why);
    if (!valid(PsuKey::OutputOn, output ? 1.0f : 0.0f, setO)) return refuse(why);
    if (!valid(PsuKey::KeyLock, keyLock ? 1.0f : 0.0f, setL)) return refuse(why);

    // Apply in an order that cannot brown-out a load: the limits move before the
    // output is switched on, and the output goes off before anything else when
    // that is what was asked. Key lock last, because locking the panel while a
    // write is still in flight is how the supply refuses the rest.
    ModbusError err = ModbusError::NoError;
    const bool turningOff = setO && !output;

    if (err == ModbusError::NoError && turningOff)
        err = psu.Write(PsuKey::OutputOn, 0.0f);

    if (err == ModbusError::NoError && setV)
        err = psu.Write(PsuKey::SetVoltage, voltage);
    if (err == ModbusError::NoError && setI)
        err = psu.Write(PsuKey::SetCurrent, current);

    if (err == ModbusError::NoError && setO && !turningOff)
        err = psu.Write(PsuKey::OutputOn, 1.0f);

    if (err == ModbusError::NoError && setB)
        err = psu.Write(PsuKey::Backlight, static_cast<float>(backlight));
    if (err == ModbusError::NoError && setL)
        err = psu.Write(PsuKey::KeyLock, keyLock ? 1.0f : 0.0f);

    // A Modbus failure is meaning, not form: it goes in the reply where it can
    // say which error it was, rather than becoming a framework REJECT.
    auto resp = ctx.reply.object();
    resp.field("ok", err == ModbusError::NoError);
    if (err != ModbusError::NoError)
        resp.field("error", ModbusErrorToString(err));

    // Echo what the supply now holds, so a caller needs no follow-up `psu get`.
    // Only the keys this supply actually has — the same reply on a supply
    // without a backlight simply has no `backlight` field.
    if (psu.Has(PsuKey::SetVoltage)) resp.field("setVoltage", psu.Number(PsuKey::SetVoltage));
    if (psu.Has(PsuKey::SetCurrent)) resp.field("setCurrent", psu.Number(PsuKey::SetCurrent));
    if (psu.Has(PsuKey::OutputOn))   resp.field("outputOn", psu.Flag(PsuKey::OutputOn));
    if (psu.Has(PsuKey::KeyLock))    resp.field("keyLock", psu.Flag(PsuKey::KeyLock));
    if (psu.Has(PsuKey::Backlight))
        resp.field("backlight", static_cast<int32_t>(psu.Number(PsuKey::Backlight)));
    return RequestError::Ok;
}
