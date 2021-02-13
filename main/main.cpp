#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "ft800.h"
#include "../components/tft/Color.h"
#include "../components/freertos_cpp/task.h"

#include "lwip/apps/sntp.h"
#include "lwip/apps/sntp_opts.h"

#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

extern "C" {
   void app_main();
}


class Pen
{
public:
	Color color = Color(255,0 ,0);
	int Width = 1;

	Pen()
	{

	}

	Pen(uint8_t r, uint8_t g, uint8_t b, int width = 1)
	{
		color = Color(r, g, b);
		Width = width;
	}
};

class Rectangle
{
public:
	int X = 50;
	int Y = 50;
	int Width = 100;
	int Height = 15;

	Rectangle()
	{

	}

	Rectangle(int x, int y, int width, int height)
	{
		X = x;
		Y = y;
		Width = width;
		Height = height;
	}
};

class Font
{
public:
	int ft800Font = 16;

};

class Graphics
{
public:
	Graphics(){}
	virtual ~Graphics(){}
	virtual void DrawLine(Pen pen, int x1, int y1, int x2, int y2) = 0;
	virtual void DrawRectangle(Pen pen, Rectangle rect) = 0;
	virtual void FillRectangle(Color c, Rectangle rect, int curve) = 0;
	virtual void FillCircle(Color c, int x, int y, int size) = 0;
	virtual void DrawString(Color c, Rectangle rect, std::string text, Font font) = 0;

};




class Control
{
protected:
	virtual void Paint(Graphics *graphics) = 0;

public:
	Control(){}
	virtual ~Control(){}

	void Draw(Graphics *graphics)
	{
		Paint(graphics);
	}
};


class Label : public Control
{
public:
	int X = 50;
	int Y = 50;
	int Width = 100;
	int Height = 20;
	Pen BorderPen = Pen(64, 64, 64, 1);
	std::string Text = "Label";
	Font font;
	Color ForeColor = Color(128, 128, 128);

	Label(){}

	Label(int x, int y, std::string text)
	{
		X = x;
		Y = y;
		Text = text;
	}

protected:
	void Paint(Graphics *graphics)
	{
		graphics->DrawRectangle(BorderPen, Rectangle(X, Y, Width, Height));

		Rectangle textRect(	X + 4,
							Y + 4,
							Width - 8,
							Height - 8);

		graphics->DrawString(ForeColor, textRect, Text, font);
	}
};

static int btnCnt = 0;

class Button : public Control
{

	int tag = btnCnt++;
public:
	int X = 150;
	int Y = 150;
	int Width = 100;
	int Height = 50;
	std::string Text = "Button";
	Font font;


protected:
	void Paint(Graphics *graphics)
	{

		//(int16_t x, int16_t y, int16_t w, int16_t h, int16_t font, uint16_t options, const char* str)

		cmd(TAG(5));
		cmd_button(X, Y, Width, Height, font.ft800Font, 0, Text.c_str());

		/*
		graphics->DrawRectangle(BorderPen, Rectangle(X, Y, Width, Height));

		Rectangle textRect(	X + 4,
							Y + 4,
							Width - 8,
							Height - 8);

		graphics->DrawString(ForeColor, textRect, Text, font);
		*/
	}
};





class FT800 : public Graphics
{

	void Work(void *arg)
	{
		uint32_t pTag = 0;
		while(1)
		{
			uint32_t tag = HOST_MEM_RD32(REG_TOUCH_TAG);

			if(tag != pTag)
			{
				pTag = tag;
				ESP_LOGI("main", "TAG = %d", tag);
			}


			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
	}

	FreeRTOS::Task task = FreeRTOS::Task("ft800", 1024 * 16, 7, this, &FT800::Work);



public:
	std::vector<Control*> Controls;

	FT800()
	{
		while(initFT800());
		//task.Run(NULL);

		//cmd(CMD_CALIBRATE);
	}

	void DrawLine(Pen pen, int x1, int y1, int x2, int y2)
	{
		cmd(COLOR_RGB(pen.color.R, pen.color.G, pen.color.B));
		cmd(LINE_WIDTH(pen.Width * 16));
		cmd(BEGIN(LINES));
		cmd(VERTEX2F(x1 * 16, y1 * 16));
		cmd(VERTEX2F(x2 * 16, y2 * 16));
		cmd(END());
	}

	void DrawRectangle(Pen pen, Rectangle rect)
	{
		DrawLine(pen, rect.X, rect.Y, rect.X + rect.Width, rect.Y);									//Top
		DrawLine(pen, rect.X + rect.Width, rect.Y, rect.X + rect.Width, rect.Y + rect.Height);		//Right
		DrawLine(pen, rect.X + rect.Width, rect.Y + rect.Height, rect.X, rect.Y + rect.Height);		//Bottom
		DrawLine(pen, rect.X, rect.Y, rect.X, rect.Y + rect.Height);								//Left
	}

	void FillRectangle(Color c, Rectangle rect, int curve)
	{
		cmd(COLOR_RGB(c.R, c.G, c.B));
		cmd(LINE_WIDTH(curve * 16));
		cmd(BEGIN(RECTS));
		cmd(VERTEX2F(rect.X * 16, rect.Y * 16));
		cmd(VERTEX2F((rect.X + rect.Width) * 16, (rect.Y + rect.Height) * 16));
		cmd(END());
	}

	void FillCircle(Color c, int x, int y, int size)
	{
		cmd(COLOR_RGB(c.R, c.G, c.B));
		cmd(POINT_SIZE(size * 16));
		cmd(BEGIN(FTPOINTS));
		cmd(VERTEX2F(x * 16, y * 16));
		cmd(END());
	}

	void Fill(Color c)
	{
		cmd(CLEAR_COLOR_RGB(c.R, c.G, c.B));
		cmd(CLEAR(1,1,1));
	}

	void DrawString(Color c, Rectangle rect, std::string text, Font font)
	{
		//Check if string fits within rectangle. No idea how to do this???
		cmd_text(rect.X, rect.Y, font.ft800Font, 0, text.c_str());
	}

	void Draw()
	{
		cmd(CMD_DLSTART);
		Fill(Color(0, 0, 0));

		for(int i=0; i<Controls.size(); i++)
			Controls[i]->Draw(this);

		cmd(DISPLAY());
		cmd(CMD_SWAP);
	}
};




/*
void Test()
{
	FT800 ft800;
	Label label;
	ft800.Controls.push_back(&label);

	Button button;
	ft800.Controls.push_back(&button);

	ft800.Draw();


	uint32_t pTag = 0xAA;
	while(1)
	{
		uint32_t tag = HOST_MEM_RD32(REG_TOUCH_TAG);

		if(tag != pTag)
		{
			pTag = tag;
			ESP_LOGI("main", "TAG = %d", tag);
		}


		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

	while(1)
		vTaskDelay(30000 / portTICK_PERIOD_MS);
}
*/

void Draw()
{
	cmd(CMD_DLSTART);
	cmd(CLEAR_COLOR_RGB(0,0,0));
	cmd(CLEAR(1,1,1));
	cmd_gradient(0,0,0xA1E1FF, 0,250,0x000080);
	cmd_text(10,245, 27,0, "Designed by: Akos Pasztor");
	cmd_text(470,250, 26,OPT_RIGHTX, "http://akospasztor.com");
	cmd(COLOR_RGB(0xDE,0x00,0x08));
	cmd_text(240,40, 31,OPT_CENTERX, "FT800 Demo");
	cmd(COLOR_RGB(255,255,255));


	int x = 5;

	cmd(TAG(5));
	cmd_fgcolor(0x228B22);
	cmd_button(x+=100, 150, 80, 50, 28, 0, "5");

	cmd(TAG(6));
	cmd_fgcolor(0x228B22);
	cmd_button(x+=100, 150, 80, 50, 28, 0, "6");

	cmd(TAG(7));
	cmd_fgcolor(0x228B22);
	cmd_button(x+=100, 150, 80, 50, 28, 0, "7");

	cmd(TAG(8));
	cmd_fgcolor(0x228B22);
	cmd_button(x+=100, 150, 80, 50, 28, 0, "8");


	cmd(DISPLAY());
	cmd(CMD_SWAP);
}

void Calibrate()
{
	cmd(CMD_DLSTART);
	cmd(CLEAR(1,1,1));
	cmd_text(80, 30, 27, OPT_CENTER, "Please tap on the dot");
	cmd(CMD_CALIBRATE);
	while(!cmd_ready())
	{
		ESP_LOGI("main", "NOT DONE");
		vTaskDelay(1000/ portTICK_PERIOD_MS);
	}

	ESP_LOGI("main", "CALI DONE");

}

void Dot(int x, int y)
{
	cmd(COLOR_RGB(255, 255, 255));
	cmd(POINT_SIZE(5 * 16));
	cmd(BEGIN(FTPOINTS));
	cmd(VERTEX2F(x * 16, y * 16));
	cmd(END());
}



void CaliDot(int dx, int dy, int *tx, int *ty)
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


void axb(float x1, float x2, float y1, float y2, float *scale, float *offset)
{
	float dy = y2 - y1;
	float dx = x2 - x1;
	*scale = dy / dx;
	*offset = y1 - *scale * x1;
}


void HomeBrewCalibrate()
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

	float xScale;
	float xOffset;
	float yScale;
	float yOffset;

	axb(x1adc, x2adc, x1px, x2px, &xScale, &xOffset);
	axb(y1adc, y2adc, y1px, y2px, &yScale, &yOffset);



	ESP_LOGI("main", "%f %f %f %f", xScale, xOffset, yScale, yOffset);

	while(1)
	{

		uint32_t tag = HOST_MEM_RD32(REG_TOUCH_DIRECT_XY);

		if((tag & 0x8000000) == 0)
		{
			int yadc = tag & 0x000003FF;		// 0000 0000 0000 0000    0000 0011 1111 1111
			int xadc = (tag & 0x03FF0000)>>16;		// 0000 0011 1111 1111    0000 0000 0000 0000

			float x = xadc * xScale + xOffset;
			float y = yadc * yScale + yOffset;


			cmd(CMD_DLSTART);
			cmd(CLEAR(1,1,1));
			Dot(x, y);
			cmd(DISPLAY());
			cmd(CMD_SWAP);

		}

		while(!cmd_ready())
			vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}


void Test()
{
	while(initFT800());

	HomeBrewCalibrate();

	Draw();

	while(1)
	{

		uint32_t tag = HOST_MEM_RD32(REG_TOUCH_DIRECT_XY);

		if((tag & 0x8000000) == 0)
		{
			int x = tag & 0x000003FF;		// 0000 0000 0000 0000    0000 0011 1111 1111
			int y = tag & 0x03FF0000;		// 0000 0011 1111 1111    0000 0000 0000 0000

			ESP_LOGI("main", "%d %d", x, y>>16);
		}


		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

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






