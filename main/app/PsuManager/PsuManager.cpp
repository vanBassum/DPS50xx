#include "PsuManager.h"
#include "BoardContext.h"
#include "StruxProvider.h"
#include "SettingsManager.h"
#include "CommandManager.h"
#include "UiManager.h"
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
                     psu.Number(PsuKey::OutputVoltage), psu.Number(PsuKey::OutputCurrent),
                     psu.Number(PsuKey::OutputPower), psu.Number(PsuKey::InputVoltage),
                     psu.Flag(PsuKey::OutputEnabled) ? "ON" : "OFF",
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

    for (const PsuCapability& c : psu)
    {
        // A capability with no telemetry name is not recorded, and one the
        // supply currently has no value for is not invented.
        if (c.telemKey == nullptr || !c.available)
            continue;
        if (c.kind == PsuKind::Bool)
            point.Field(c.telemKey, c.AsBool());
        else
            point.Field(c.telemKey, c.value);
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

/// The wire name for a capability's kind. A converter, so it lives here at the
/// edge rather than on the role — same split as SettingsManager keeping its
/// JSON out of Setting.
static const char* KindName(PsuKind kind)
{
    switch (kind)
    {
    case PsuKind::Bool:     return "bool";
    case PsuKind::Enum:     return "enum";
    case PsuKind::Duration: return "duration";
    case PsuKind::Number:
    default:                return "number";
    }
}

/// Which section of a UI a capability belongs to. The DRIVER decides this — see
/// PsuGroup — so a dashboard needs no list of keys to know that a preset is not
/// a measurement and a trip threshold is not a live value.
static const char* GroupName(PsuGroup group)
{
    switch (group)
    {
    case PsuGroup::Session:    return "session";
    case PsuGroup::Protection: return "protection";
    case PsuGroup::Info:       return "info";
    case PsuGroup::Core:
    default:                   return "core";
    }
}

RequestError PsuManager::Cmd_Get(CommandContext& ctx)
{
    RETURN_IF_ERROR(ctx.readArgs());

    Psu& psu = app_.getBoard().GetPsu();

    // The reply DESCRIBES itself: every capability carries its own label, unit,
    // kind, group and writable range, so the browser renders a supply it was
    // never told about — and this handler names neither a register nor a
    // supply. What arrives here is already semantic; see interfaces/Psu.h.
    auto resp = ctx.reply.object();
    resp.field("online", psu.IsOnline());

    auto capabilities = resp.object("capabilities");
    for (const PsuCapability& c : psu)
    {
        auto entry = capabilities.object(c.key);
        entry.field("label", c.label);
        entry.field("unit", c.unit);
        entry.field("kind", KindName(c.kind));
        entry.field("group", GroupName(c.group));
        entry.field("access", c.Writable() ? "rw" : "r");

        // A Duration is a whole number of seconds, so it goes on the wire as an
        // integer rather than as a float that would print 2720.00. uint32 is
        // 136 years of seconds; a 64-bit path through lib/protocol would be
        // fork debt for a range no bench supply reaches.
        if (c.kind == PsuKind::Duration)
            entry.field("value", static_cast<uint32_t>(c.value));
        else
            entry.field("value", static_cast<float>(c.value));

        // "The supply has no measurement for this right now" — an absent probe
        // answering its sentinel, or an optional block that did not reply. A UI
        // shows "—" instead of presenting either as a reading.
        if (!c.available)
            entry.field("available", false);

        // An enum's codes are named by the supply that defines them, which is
        // what let PROTECTION_LABELS die in the frontend. The whole TABLE goes
        // with it, so a selector can be drawn without knowing that 0 is M0.
        if (const char* label = c.ValueLabel())
            entry.field("valueLabel", label);
        if (c.kind == PsuKind::Enum && c.enumLabels != nullptr)
        {
            auto options = entry.array("options");
            for (uint8_t i = 0; i < c.enumCount; ++i)
                options.value(c.enumLabels[i]);
        }

        // The bounds the browser draws its inputs from, and the same ones the
        // handlers below refuse against.
        if (c.Writable())
        {
            entry.field("min", c.min);
            entry.field("max", c.max);
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
    bool     output    = psu.Flag(PsuKey::OutputEnabled);
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
    const bool setO = output    != psu.Flag(PsuKey::OutputEnabled);
    const bool setL = keyLock   != psu.Flag(PsuKey::KeyLock);

    char why[96] = {};
    auto valid = [&](const char* key, float value, bool changed) {
        if (!changed)
            return true;
        const PsuCapability* c = psu.Find(key);
        if (c == nullptr)
        {
            snprintf(why, sizeof(why), "this supply has no '%s'", key);
            return false;
        }
        if (!c->InRange(value))
        {
            snprintf(why, sizeof(why), "%s out of range (%g-%g%s%s)",
                     c->label, c->min, c->max, c->unit[0] ? " " : "", c->unit);
            return false;
        }
        return true;
    };

    if (!valid(PsuKey::SetVoltage, voltage, setV)) return refuse(why);
    if (!valid(PsuKey::SetCurrent, current, setI)) return refuse(why);
    if (!valid(PsuKey::Backlight, static_cast<float>(backlight), setB)) return refuse(why);
    if (!valid(PsuKey::OutputEnabled, output ? 1.0f : 0.0f, setO)) return refuse(why);
    if (!valid(PsuKey::KeyLock, keyLock ? 1.0f : 0.0f, setL)) return refuse(why);

    // Apply in an order that cannot brown-out a load: the limits move before the
    // output is switched on, and the output goes off before anything else when
    // that is what was asked. Key lock last, because locking the panel while a
    // write is still in flight is how the supply refuses the rest.
    ModbusError err = ModbusError::NoError;
    const bool turningOff = setO && !output;

    if (err == ModbusError::NoError && turningOff)
        err = psu.Write(PsuKey::OutputEnabled, 0.0f);

    if (err == ModbusError::NoError && setV)
        err = psu.Write(PsuKey::SetVoltage, voltage);
    if (err == ModbusError::NoError && setI)
        err = psu.Write(PsuKey::SetCurrent, current);

    if (err == ModbusError::NoError && setO && !turningOff)
        err = psu.Write(PsuKey::OutputEnabled, 1.0f);

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
    // Echoed under the CAPABILITY keys, so a caller folds the reply straight
    // back into what `psu get` gave it.
    if (psu.Has(PsuKey::SetVoltage))    resp.field("setVoltage", psu.Number(PsuKey::SetVoltage));
    if (psu.Has(PsuKey::SetCurrent))    resp.field("setCurrent", psu.Number(PsuKey::SetCurrent));
    if (psu.Has(PsuKey::OutputEnabled)) resp.field("outputEnabled", psu.Flag(PsuKey::OutputEnabled));
    if (psu.Has(PsuKey::KeyLock))       resp.field("keyLock", psu.Flag(PsuKey::KeyLock));
    if (psu.Has(PsuKey::Backlight))
        resp.field("backlight", static_cast<int32_t>(psu.Number(PsuKey::Backlight)));
    return RequestError::Ok;
}

RequestError PsuManager::Cmd_Write(CommandContext& ctx)
{
    char key[32] = {};
    float value = 0.0f;
    RETURN_IF_ERROR(ctx.readArgs(
        Required("key", key),
        Required("value", value)
    ));

    Psu& psu = app_.getBoard().GetPsu();

    auto refuse = [&ctx](const char* why) {
        auto resp = ctx.reply.object();
        resp.field("ok", false);
        resp.field("error", why);
        return RequestError::Ok;
    };

    // Everything this handler knows about what it is writing comes off the
    // CAPABILITY: whether the supply has it, whether it can be written, and
    // what range it accepts. Which register that becomes — or whether it is one
    // register, or two, or none — never reaches here. So the same code
    // configures an XY6020L over-voltage trip and whatever the next supply has.
    PsuCapability* c = psu.Find(key);
    if (c == nullptr)
        return refuse("this supply has no such capability");
    if (!c->Writable())
        return refuse("capability is read-only");

    char why[96];
    if (!c->InRange(value))
    {
        snprintf(why, sizeof(why), "%s out of range (%g-%g%s%s)",
                 c->label, c->min, c->max, c->unit[0] ? " " : "", c->unit);
        return refuse(why);
    }

    const ModbusError err = psu.Write(*c, value);

    auto resp = ctx.reply.object();
    resp.field("ok", err == ModbusError::NoError);
    if (err != ModbusError::NoError)
        resp.field("error", ModbusErrorToString(err));
    resp.field("key", c->key);
    resp.field("value", static_cast<float>(c->value));
    return RequestError::Ok;
}
