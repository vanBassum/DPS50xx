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
#include "driver/gpio.h"

#include <string.h>

#include "DPS5020.h"
#include "esplib/onewire/onewire.h"
#include "esplib/misc/property.h"
#include "esplib/openapi/openapi.h"

extern "C" {
   void app_main();
}


class Status : public PropertyContainer
{
public:
	Prop<float> USet = Prop<float>(this, "USet");
	Prop<float> ISet = Prop<float>(this, "ISet");
	Prop<float> UOut = Prop<float>(this, "UOut");
	Prop<float> IOut = Prop<float>(this, "IOut");
	Prop<float> UAnode = Prop<float>(this, "UAnode");
	Prop<float> Temp = Prop<float>(this, "Temp");

	std::string GetName() override
	{
		return "Status";
	}
};


class Settings : public PropertyContainer
{
public:
	Prop<float> Resistance 			= Prop<float>(this, "Resistance", 0);
	Prop<int> Protection 			= Prop<int>(this, "Protection", 0);
	Prop<int> ThresholdVoltage 		= Prop<int>(this, "ThresholdVoltage", 5);
	Prop<int> ShutdownVoltage 		= Prop<int>(this, "ShutdownVoltage", 6);

	void Copy(Settings s)
	{
		Resistance.Set(s.Resistance.Get());
		Protection.Set(s.Protection.Get());
		ThresholdVoltage.Set(s.ThresholdVoltage.Get());
	}

	std::string GetName() override
	{
		return "Settings";
	}
};


Settings settings;
Status status;



Status GetStatus()
{
	return status;
}

Settings GetSettings()
{
	return settings;
}

bool SetSettings(Settings s)
{
	settings.Copy(s);
	return true;
}

void IOutChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 100;
	status.IOut.Set(val);
}

void UOutChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 100;
	status.UOut.Set(val);

	float voltageDrop = settings.Resistance.Get() * status.IOut.Get();
	status.UAnode.Set(voltageDrop + status.UOut.Get());
}

void ISetChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 100;
	status.ISet.Set(val);
}

void USetChanged(Modbus::Property *prop)
{
	float val = prop->Get() / 100;
	status.USet.Set(val);
}

void TChanged(float val)
{
	status.Temp.Set(val);
}


void Test()
{
	DPS5020 dps;
	Swagger::OpenAPI api;

	api.RegisterGetFunction("/Status", &GetStatus);
	api.RegisterGetFunction("/Settings", &GetSettings);
	api.RegisterPostFunction("/Settings", &SetSettings);

	api.GenerateSwaggerJSON();

	dps.UOut.OnChange.Bind(UOutChanged);
	dps.IOut.OnChange.Bind(IOutChanged);
	dps.USet.OnChange.Bind(USetChanged);
	dps.ISet.OnChange.Bind(ISetChanged);


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

		if(settings.Protection.Get())
		{
			if(status.UAnode.Get() > settings.ShutdownVoltage.Get())
			{
				dps.Power.Set(0);
			}
			else if(status.UAnode.Get() > settings.ThresholdVoltage.Get())
			{
				//First lower the current.
				//Second lower the voltage.
				//If nothing helps, cut the power.


				if(status.IOut.Get() > 1)
					dps.ISet.Set((status.IOut.Get() - 1) / 100);
				else if(status.UOut.Get() > 1)
					dps.UOut.Set((status.UOut.Get() - 1) / 100);
				else
					dps.Power.Set(0);
			}
		}

		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
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






