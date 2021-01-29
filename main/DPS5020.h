#pragma once

#include <vector>

#include "../components/modbus/modbus.h"
#include "../components/freertos_cpp/freertos.h"



class DPS5020
{
	Modbus::Modbus* modbus = NULL;
	std::vector<Modbus::Property*> propList;
	FreeRTOS::Task *syncTask = NULL;
public:
	Modbus::Property USet	 = Modbus::Property(0, &propList);
	Modbus::Property ISet	 = Modbus::Property(1, &propList);
	Modbus::Property UOut	 = Modbus::Property(2, &propList);
	Modbus::Property IOut	 = Modbus::Property(3, &propList);
	Modbus::Property Power	 = Modbus::Property(4, &propList);
	Modbus::Property UIn	 = Modbus::Property(5, &propList);
	Modbus::Property Lock	 = Modbus::Property(6, &propList);
	Modbus::Property Protect = Modbus::Property(7, &propList);
	Modbus::Property CVCC	 = Modbus::Property(8, &propList);
	Modbus::Property ONOFF	 = Modbus::Property(9, &propList);
	Modbus::Property BLight	 = Modbus::Property(10, &propList);
	Modbus::Property Model	 = Modbus::Property(11, &propList);
	Modbus::Property Version = Modbus::Property(12, &propList);

private:
	void SyncTask(void *arg)
	{
		while(1)
		{
			if(modbus != NULL)
			{
				Modbus::Exception ex = Modbus::Exception::NoError;

				for(int i=0; i<propList.size() && ex == Modbus::Exception::NoError;i++)
					ex = propList[i]->Sync(modbus, 2000);

				if(ex != Modbus::Exception::NoError)
					ESP_LOGE("Intesis", "MODBUS error while syncing %d", (int)ex);
			}
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}



public:
	DPS5020()
	{
		modbus = new Modbus::ModbusRTU(UART_NUM_2, 9600);
		syncTask = new FreeRTOS::Task("DPS5020", 7, 1024, this, &DPS5020::SyncTask);
		syncTask->Run(NULL);
	}

	~DPS5020()
	{
		delete modbus;
		delete syncTask;
	}
};







