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
#include "nvs.h"

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

class Control : public PropertyContainer
{
public:
	Prop<float> USet = Prop<float>(this, "USet");
	Prop<float> ISet = Prop<float>(this, "ISet");

	std::string GetName() override
	{
		return "Control";
	}
};

class Settings : public PropertyContainer
{
public:
	Prop<float> Resistance 			= Prop<float>(this, "Resistance", 0);
	Prop<int> Protection 			= Prop<int>(this, "Protection", 0);
	Prop<int> ThresholdVoltage 		= Prop<int>(this, "ThresholdVoltage", 5);
	Prop<float> SaveVoltage 		= Prop<float>(this, "SaveVoltage", 4);
	Prop<float> SaveCurrent 		= Prop<float>(this, "SaveCurrent", 0.1);

	bool VerifyCopy(Settings s)
	{
		bool result = false;
		Resistance.Set(s.Resistance.Get());
		Protection.Set(s.Protection.Get());
		ThresholdVoltage.Set(s.ThresholdVoltage.Get());
		SaveCurrent.Set(s.SaveCurrent.Get());

		if(s.SaveVoltage.Get() < s.ThresholdVoltage.Get())
		{
			SaveVoltage.Set(s.SaveVoltage.Get());
			result = true;
		}
		else
		{
			SaveVoltage.Set(s.ThresholdVoltage.Get() - s.ThresholdVoltage.Get() /10);
		}
		return result;
	}

	std::string GetName() override
	{
		return "Settings";
	}
};


Settings settings;
Status status;
DPS5020 dps;


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
	bool result = settings.VerifyCopy(s);
	if(result)
	{
		settings.SaveToNVS("nvs", "settings");
	}
	return result;
}

bool SetControl(Control s)
{
	dps.USet.Set(s.USet.Get() * 100);
	dps.ISet.Set(s.ISet.Get() * 100);
	return true;
}


void CalcUAnode()
{
	float voltageDrop = settings.Resistance.Get() * status.IOut.Get();
	status.UAnode.Set(voltageDrop + status.UOut.Get());
}

void UOutChanged(Modbus::Property *prop)
{
	float val = prop->Get();
	status.UOut.Set(val / 100);
	CalcUAnode();
}

void IOutChanged(Modbus::Property *prop)
{
	float val = prop->Get();
	status.IOut.Set(val / 100);
	CalcUAnode();
}

void USetChanged(Modbus::Property *prop)
{
	float val = prop->Get();
	status.USet.Set(val / 100);
}

void ISetChanged(Modbus::Property *prop)
{
	float val = prop->Get();
	status.ISet.Set(val / 100);
}

void TChanged(float val)
{
	status.Temp.Set(val);
}


void Test()
{
	settings.LoadFromNVS("nvs", "settings");


	Swagger::OpenAPI api;

	api.RegisterGetFunction("/Status", &GetStatus);
	api.RegisterGetFunction("/Settings", &GetSettings);
	api.RegisterPostFunction("/Settings", &SetSettings);
	api.RegisterPostFunction("/Control", &SetControl);
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
			float uOut = status.UOut.Get();
			float uAnode = status.UAnode.Get();
			float uAnodeMax = settings.ThresholdVoltage.Get();
			float iOut = status.IOut.Get();

			if(uAnode > uAnodeMax)
			{

				if(uOut > 1)
				{
					float uExceeded = uAnodeMax - uAnode;
					ESP_LOGI("Main", "Max anode voltage exceeded (%.2f > %.2f). lowering voltage by %.2fV percent to prevent anode damage. ", uAnode, uAnodeMax, uExceeded);
					dps.USet.Set((uOut - (uOut/10)) * 100);
				}
				else
				{
					ESP_LOGI("Main", "Max anode voltage exceeded (%.2f > %.2f. lowering to safetylevels.", uAnode, uAnodeMax);
					dps.USet.Set(settings.SaveVoltage.Get() * 100);
					dps.ISet.Set(settings.SaveCurrent.Get() * 100);
				}
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






