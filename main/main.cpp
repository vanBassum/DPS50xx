#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <string.h>
#include "lwip/apps/sntp.h"
#include "lwip/apps/sntp_opts.h"
#include "Commands.h"
#include <cJSON.h>

#include "../components/jbvprotocol/jbvclient.h"
#include "../components/onewire/onewire.h"
#include "DPS5020.h"

JBVClient client(SoftwareID::DPS50xx);


extern "C" {
   void app_main();
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


void WriteLog(char* str)
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "FileName", "DPSLog2.json");
	cJSON_AddStringToObject(root, "Data", str);
	char *bb = cJSON_Print(root);

	Frame frame = Frame();
	frame.RxID = 1;
	frame.CommandID = 2;
	frame.SetData((uint8_t*)bb, strlen(bb));

	free(bb);
	cJSON_Delete(root);
	client.SendFrame(&frame);
}



void Log(char* type, cJSON* obj)
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "Type", type);
	cJSON_AddNumberToObject(root, "TimeStamp", DateTime::Now().MKTime());
	cJSON_AddItemToObject(root, "Data", obj);
	char *bb = cJSON_Print(root);
	WriteLog(bb);
	free(bb);
	cJSON_Delete(root);
}


void IOutChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 10;
	Log("IChange", cJSON_CreateNumber(val));
	ESP_LOGI("Test", "IOutChanged = %f", val);
}

void UOutChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 10;
	Log("UChange", cJSON_CreateNumber(val));
	ESP_LOGI("Test", "UOutChanged = %f", val);
}

void TChanged(float val)
{
	Log("TChange", cJSON_CreateNumber(val));
	ESP_LOGI("Test", "TChanged = %f", val);
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

	gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_2, 0);



	TCPConnection con;
	con.Connect("192.168.11.50", 32770, true);
	client.SetConnection(&con);
	client.HandleFrame.Bind(HandleFrame);

	DPS5020 dps;
	dps.UOut.OnChange.Bind(UOutChanged);


	OneWire::Bus onewire(GPIO_NUM_4);
	OneWire::DS18B20 *tempSensors[10];

	int devCnt = onewire.Search((OneWire::Device **)tempSensors, 10, OneWire::Devices::DS18B20);
	double pTemp = 0;
	while(1)
	{
		if(devCnt >= 1)
		{
			float t = tempSensors[0]->GetTemp();
			if(pTemp != t)
			{
				TChanged(t);
				pTemp = t;
			}
		}
		vTaskDelay(30000 / portTICK_PERIOD_MS);
	}


	/*
	con.Connect("192.168.11.14", 1000, true);
	client.SetConnection(&con);
	client.HandleFrame = new Callback<void, JBVClient*, Frame*>(HandleFrame);


	dps.IOut.OnChange.Bind(&IOutChanged);
	dps.UOut.OnChange.Bind(&UOutChanged);

	ds18b20_init(GPIO_NUM_4);

	int itemp = 0;

    while(true)
    {
    	float temp = ds18b20_get_temp();
    	int newTemp = temp * 10;
    	if(itemp != newTemp)
    	{
    		itemp = newTemp;
    		Log("TChange", cJSON_CreateNumber(itemp));
    		ESP_LOGI("Test", "TempChanged = %d", itemp);
    	}
    	vTaskDelay(5000 / portTICK_PERIOD_MS);
    }*/
}






