/*
 * FT800.cpp
 *
 *  Created on: Feb 14, 2021
 *      Author: Bas
 */

#include "FT800.h"

#include "esp_log.h"

/*
void axb(float x1, float x2, float y1, float y2, float *scale, float *offset)
{
	float dy = y2 - y1;
	float dx = x2 - x1;
	*scale = dy / dx;
	*offset = y1 - *scale * x1;
}
*/

FT800::FT800()
{
	InitBus();
	task = new FreeRTOS::NotifyableTask("FT800", 7, 1024 * 8, this, &FT800::Work);
	task->Run(NULL);
	//task->Notify((uint32_t)Notifications::ScreenCalibrate);
}


void FT800::Work(void *arg)
{
	ESP_LOGI("FT800", "Task started");

	while(initFT800());

	Control *touchedControl = NULL;

	while(1)
	{
		Notifications notifications;
		task->GetNotifications((uint32_t *)&notifications, touchPollTime);


		switch(notifications)
		{

		case Notifications::Refresh:
			Begin();
			controlsMutex.Take();
			for(int i=0; i<controls.size(); i++)
				controls[i]->Paint(this, window);
			controlsMutex.Give();
			End();
			break;

		case Notifications::ScreenCalibrate:
			//Calibrate();
			break;
		}

		uint32_t touchReg = HOST_MEM_RD32(REG_TOUCH_DIRECT_XY);
		bool touch = !(touchReg & 0x80000000);

		if(touch)
		{
			float x = (touchReg & 0x03FF0000) >> 16;
			float y = (touchReg & 0x000003FF);
			x = x * xScale + xOffset;
			y = y * yScale + yOffset;
			controlsMutex.Take();
			for(int i=controls.size() - 1; i>=0; i--)
			{
				if(controls[i]->Collides(x, y))
				{
					//We have a collision, is it with the same object
					if(touchedControl != controls[i])
					{
						if(touchedControl != NULL)
							touchedControl->TouchUp(touchedControl);
						touchedControl = controls[i];
						touchedControl->TouchDown(touchedControl);
					}
					break;
				}
			}
			controlsMutex.Give();
		}
		else
		{
			if(touchedControl != NULL)
			{
				touchedControl->TouchUp(touchedControl);
				touchedControl = NULL;
			}
		}
	}
}


void FT800::Begin()
{
	cmd(CMD_DLSTART);
	cmd(CLEAR(1,1,1));
}

void FT800::End()
{
	cmd(DISPLAY());
	cmd(CMD_SWAP);
}

void FT800::Refresh()
{
	task->Notify((uint32_t)Notifications::Refresh);
}

void FT800::AddControl(Control *ctrl)
{
	controlsMutex.Take();
	controls.push_back(ctrl);
	controlsMutex.Give();
	ctrl->RedrawRequest.Bind(this, &FT800::Refresh);
	Refresh();
}

/*
void FT800::CaliDot(int dx, int dy, int *tx, int *ty)
{
	uint32_t tag = -1;

	cmd(CMD_DLSTART);
	cmd(CLEAR(1,1,1));
	Dot(dx, dy);
	cmd(DISPLAY());
	cmd(CMD_SWAP);

	while((tag & 0x8000000) != 0)
	{
		tag = HOST_MEM_RD32(REG_TOUCH_DIRECT_XY);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}


	*ty = tag & 0x000003FF;				// 0000 0000 0000 0000    0000 0011 1111 1111
	*tx = (tag & 0x03FF0000) >> 16;		// 0000 0011 1111 1111    0000 0000 0000 0000

	while((tag & 0x8000000) == 0)
	{
		tag = HOST_MEM_RD32(REG_TOUCH_DIRECT_XY);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

}

void FT800::Calibrate()
{
	int x1px = 10;
	int y1px = 10;
	int x2px = 470;
	int y2px = 260;
	int x1adc = 0;
	int y1adc = 0;
	int x2adc = 0;
	int y2adc = 0;

	CaliDot(x1px, y1px, &x1adc, &y1adc);
	CaliDot(x2px, y2px, &x2adc, &y2adc);
	axb(x1adc, x2adc, x1px, x2px, &xScale, &xOffset);
	axb(y1adc, y2adc, y1px, y2px, &yScale, &yOffset);

	ESP_LOGI("main", "%f %f %f %f", xScale, xOffset, yScale, yOffset);
}





void FT800::Dot(int x, int y)
{
	cmd(COLOR_RGB(255, 255, 255));
	cmd(POINT_SIZE(5 * 16));
	cmd(BEGIN(FTPOINTS));
	cmd(VERTEX2F(x * 16, y * 16));
	cmd(END());
}
*/
void FT800::DrawLine(Pen pen, int x1, int y1, int x2, int y2)
{
	cmd(COLOR_RGB(pen.color.R, pen.color.G, pen.color.B));
	cmd(LINE_WIDTH(pen.Width * 16));
	cmd(BEGIN(LINES));
	cmd(VERTEX2F(x1 * 16, y1 * 16));
	cmd(VERTEX2F(x2 * 16, y2 * 16));
	cmd(END());
}

void FT800::DrawRectangle(Pen pen, Rectangle rect)
{
	DrawLine(pen, rect.X, rect.Y, rect.X + rect.Width, rect.Y);									//Top
	DrawLine(pen, rect.X + rect.Width, rect.Y, rect.X + rect.Width, rect.Y + rect.Height);		//Right
	DrawLine(pen, rect.X + rect.Width, rect.Y + rect.Height, rect.X, rect.Y + rect.Height);		//Bottom
	DrawLine(pen, rect.X, rect.Y, rect.X, rect.Y + rect.Height);								//Left
}












