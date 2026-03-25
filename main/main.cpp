#include <stdio.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ApplicationContext.h"

static const char* TAG = "main";

ApplicationContext g_appContext;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting up...");
    g_appContext.getLogManager().Init();
    g_appContext.getSettingsManager().Init();
    g_appContext.getNetworkManager().Init();
    g_appContext.getTimeManager().Init();
    g_appContext.getCommandManager().Init();
    g_appContext.getDeviceManager().Init();
    g_appContext.getUpdateManager().Init();
    g_appContext.getWebServerManager().Init();

    // Mark firmware as valid so the bootloader doesn't roll back on next reboot
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI(TAG, "All managers initialized, firmware confirmed valid");
}
