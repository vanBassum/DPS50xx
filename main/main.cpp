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


class Graphics
{
public:
	Graphics(){}
	virtual ~Graphics(){}
	virtual void DrawLine(Pen pen, int x1, int y1, int x2, int y2) = 0;
	virtual void DrawString(Color c, int x, int y, std::string text, int height) = 0;
	virtual void FillCircle(Color c, int x, int y, int size) = 0;
	virtual void FillRectangle(Color c, int x, int y, int width, int height, int curve) = 0;

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
	int Height = 15;
	Pen BorderPen = Pen(0, 0, 0, 1);
	std::string Text = "Label";


protected:
	void Paint(Graphics *graphics)
	{
		ESP_LOGI("Label", "Paint");
		graphics->DrawLine(BorderPen, X, Y, X + Width, Y);						//Top
		graphics->DrawLine(BorderPen, X + Width, Y, X + Width, Y + Height);		//Right
		graphics->DrawLine(BorderPen, X + Width, Y + Height, X, Y + Height);	//Bottom
		graphics->DrawLine(BorderPen, X, Y, X, Y + Height);						//Left
		graphics->DrawString(Color(0, 0, 0), X, Y, Text, 12);
	}
};






class FT800 : public Graphics
{
public:
	std::vector<Control*> Controls;

	FT800()
	{
		while(initFT800());
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

	void FillCircle(Color c, int x, int y, int size)
	{
		cmd(COLOR_RGB(c.R, c.G, c.B));
		cmd(POINT_SIZE(size * 16));
		cmd(BEGIN(FTPOINTS));
		cmd(VERTEX2F(x * 16, y * 16));
		cmd(END());
	}

	void FillRectangle(Color c, int x, int y, int width, int height, int curve)
	{
		cmd(COLOR_RGB(c.R, c.G, c.B));
		cmd(LINE_WIDTH(curve * 16));
		cmd(BEGIN(RECTS));
		cmd(VERTEX2F(x * 16, y * 16));
		cmd(VERTEX2F((x + width) * 16, (y + height) * 16));
		cmd(END());
	}

	void Fill(Color c)
	{
		cmd(CLEAR_COLOR_RGB(c.R, c.G, c.B));
		cmd(CLEAR(1,1,1));
	}

	void DrawString(Color c, int x, int y, std::string text, int height)
	{

		cmd(COLOR_RGB(c.R, c.G, c.B));
		cmd(BEGIN(BITMAPS));

		for(int i=0; i<text.length(); i++)
		{
			cmd(VERTEX2II(i * 10 + x, y, 16, text[i]));
		}

		cmd(END());
	}

	void Draw()
	{
		cmd(CMD_DLSTART);
		Fill(Color(255, 255, 255));

		for(int i=0; i<Controls.size(); i++)
			Controls[i]->Draw(this);

		cmd(DISPLAY());
		cmd(CMD_SWAP);
	}





};





void Test()
{
	FT800 ft800;

	Label label;


	ft800.Controls.push_back(&label);



	ft800.Draw();

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






