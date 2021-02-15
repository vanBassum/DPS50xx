#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/sntp_opts.h"
#include <string.h>

#include "displays/FT800.h"
#include "controls/button.h"


extern "C" {
   void app_main();
}


void Up(Control *ctrl)
{
	ESP_LOGI("main", "UP");
}

void Down(Control *ctrl)
{
	ESP_LOGI("main", "DOWN");
}

void Test()
{
	FT800 ft800;

	Button btn;

	ft800.AddControl(&btn);

	btn.TouchDown.Bind(Down);
	btn.TouchUp.Bind(Up);

	while(1)
		vTaskDelay(30000 / portTICK_PERIOD_MS);
}



esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch(event->event_id)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		//con.WifiConnected();
		break;
	default:
		break;
	}

    return ESP_OK;
}






void app_main(void)
{
    nvs_flash_init();

    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );

    wifi_config_t sta_config = {};
    memcpy(sta_config.sta.ssid, "vanBassum", 10);
    memcpy(sta_config.sta.password, "pedaalemmerzak", 15);
    sta_config.sta.bssid_set = false;
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    setenv("TZ", "UTC-1UTC,M3.5.0,M10.5.0/3", 1);
	tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_init();



	Test();
	while(1)
		vTaskDelay(30000 / portTICK_PERIOD_MS);
}






