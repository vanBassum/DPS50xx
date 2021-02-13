/**
  ******************************************************************************
  * C Library for FT800 EVE module
  ******************************************************************************
  * @author  Akos Pasztor    (http://akospasztor.com)
  * @file    ft800.c
  * @brief   FT800 Display Controller Library
  *          This file contains the initialization and functions for
  *          the FT800 EVE module.
  * @info    http://www.ftdichip.com/Products/ICs/FT800.html
  ******************************************************************************
  * Copyright (c) 2014 Akos Pasztor. All rights reserved.
  ******************************************************************************
**/
#include "ft800.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"


//#define TAG	"FT800"


static const gpio_num_t GPIO_SCLK 	= (gpio_num_t)19;
static const gpio_num_t GPIO_MOSI 	= (gpio_num_t)2; //18
static const gpio_num_t GPIO_MISO 	= (gpio_num_t)5;
static const gpio_num_t GPIO_CS 	= (gpio_num_t)17;
static const gpio_num_t GPIO_INT 	= (gpio_num_t)16;
static const gpio_num_t GPIO_PD 	= (gpio_num_t)4;
static const int SPI_Frequency 		= 10000000;

spi_bus_config_t buscfg;
spi_device_interface_config_t devcfg;
spi_device_handle_t handle;


/* Delaying function */
void sysDms(uint32_t millisec)
{
	vTaskDelay(millisec / portTICK_PERIOD_MS);
}


void InitBus()
{
	esp_err_t ret;
	ESP_LOGI("FT800", "GPIO_CS=%d",GPIO_CS);
	gpio_pad_select_gpio( GPIO_CS );
	gpio_set_direction( GPIO_CS, GPIO_MODE_OUTPUT );
	gpio_set_level( GPIO_CS, 1 );
	buscfg.sclk_io_num = GPIO_SCLK;
	buscfg.mosi_io_num = GPIO_MOSI;
	buscfg.miso_io_num = GPIO_MISO;
	buscfg.quadwp_io_num = -1;
	buscfg.quadhd_io_num = -1;

	devcfg.clock_speed_hz = SPI_Frequency;
	devcfg.spics_io_num = GPIO_CS;
	devcfg.queue_size = 7;
	//devcfg.flags = SPI_DEVICE_NO_DUMMY;
	devcfg.mode = 0;

	ret = spi_bus_initialize( HSPI_HOST, &buscfg, 0);
	ESP_LOGI("FT800", "spi_bus_initialize=%d",ret);
	assert(ret==ESP_OK);
	ret = spi_bus_add_device( HSPI_HOST, &devcfg, &handle);
	ESP_LOGI("FT800", "spi_bus_add_device=%d",ret);
	assert(ret==ESP_OK);
}


bool SPI_WRT(const uint8_t* txBuf, uint8_t* rxBuf, size_t DataLength)
{
	spi_transaction_t SPITransaction;
	esp_err_t ret;

	if ( DataLength > 0 )
	{
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = txBuf;
		SPITransaction.rx_buffer = rxBuf;
		ret = spi_device_transmit( handle, &SPITransaction );
		assert(ret==ESP_OK);
	}

	return true;
}






/* Init function for an 5" LCD display */
uint8_t initFT800(void)
{
	static bool first = true;
	if(first)
	{
		InitBus();
		first = false;
	}
	uint8_t dev_id = 0;                  // Variable for holding the read device id

	gpio_set_direction(GPIO_PD, GPIO_MODE_OUTPUT);
	gpio_set_direction(GPIO_INT, GPIO_MODE_INPUT);


	gpio_set_level(GPIO_PD, 0);			// Set the PDN pin low
	sysDms(50);                          // Delay 50 ms for stability
	gpio_set_level(GPIO_PD, 1);			// Set the PDN pin high
	sysDms(50);                          // Delay 50 ms for stability



	//HOST_CMD_WRITE(CMD_ACTIVE);

	//sysDms(500);

	//Ext Clock
	HOST_CMD_WRITE(CMD_CLKEXT);         // Send CLK_EXT Command (0x44)
	HOST_CMD_WRITE(CMD_ACTIVE);

	//PLL (48M) Clock
	//HOST_CMD_WRITE(CMD_CLK48M);         // Send CLK_48M Command (0x62)

	sysDms(50);

	//Read Dev ID
	dev_id = HOST_MEM_RD8(REG_ID);      	// Read device id
	if(dev_id == 0x7C)                  	// Device ID should always be 0x7C
	{
		ESP_LOGI("FT800", "DevID OKE, screen initializing started");
	}
	else
	{
		ESP_LOGE("FT800", "DevID invalid %x", dev_id);
		return 1;
	}



	HOST_MEM_WR8(REG_GPIO, 0x00);			// Set REG_GPIO to 0 to turn off the LCD DISP signal
	HOST_MEM_WR8(REG_PCLK, 0x00);      		// Pixel Clock Output disable

	HOST_MEM_WR16(REG_HCYCLE, 548);         // Set H_Cycle to 548
	HOST_MEM_WR16(REG_HOFFSET, 43);         // Set H_Offset to 43
	HOST_MEM_WR16(REG_HSYNC0, 0);           // Set H_SYNC_0 to 0
	HOST_MEM_WR16(REG_HSYNC1, 41);          // Set H_SYNC_1 to 41
	HOST_MEM_WR16(REG_VCYCLE, 292);         // Set V_Cycle to 292
	HOST_MEM_WR16(REG_VOFFSET, 12);         // Set V_OFFSET to 12
	HOST_MEM_WR16(REG_VSYNC0, 0);           // Set V_SYNC_0 to 0
	HOST_MEM_WR16(REG_VSYNC1, 10);          // Set V_SYNC_1 to 10
	HOST_MEM_WR8(REG_SWIZZLE, 0);           // Set SWIZZLE to 0
	HOST_MEM_WR8(REG_PCLK_POL, 1);          // Set PCLK_POL to 1
	HOST_MEM_WR8(REG_CSPREAD, 1);           // Set CSPREAD to 1
	HOST_MEM_WR16(REG_HSIZE, 480);          // Set H_SIZE to 480
	HOST_MEM_WR16(REG_VSIZE, 272);          // Set V_SIZE to 272



	/* configure touch & audio */
	HOST_MEM_WR8(REG_TOUCH_MODE, 0x03);     	//set touch on: continous
	HOST_MEM_WR8(REG_TOUCH_ADC_MODE, 0x01); 	//set touch mode: differential
	HOST_MEM_WR8(REG_TOUCH_OVERSAMPLE, 0x0F); 	//set touch oversampling to max
	HOST_MEM_WR16(REG_TOUCH_RZTHRESH, 5000);	//set touch resistance threshold
	HOST_MEM_WR8(REG_VOL_SOUND, 0xFF);      	//set the volume to maximum

	/* write first display list */
	HOST_MEM_WR32(RAM_DL+0, CLEAR_COLOR_RGB(0,0,0));  // Set Initial Color to BLACK
	HOST_MEM_WR32(RAM_DL+4, CLEAR(1,1,1));            // Clear to the Initial Color
	HOST_MEM_WR32(RAM_DL+8, DISPLAY());               // End Display List

	HOST_MEM_WR8(REG_DLSWAP, DLSWAP_FRAME);           // Make this display list active on the next frame

	HOST_MEM_WR8(REG_GPIO_DIR, 0x80);                 // Set Disp GPIO Direction
	HOST_MEM_WR8(REG_GPIO, 0x80);                     // Enable Disp (if used)
	HOST_MEM_WR16(REG_PWM_HZ, 0x00FA);                // Backlight PWM frequency
	HOST_MEM_WR8(REG_PWM_DUTY, 0x80);                 // Backlight PWM duty

	HOST_MEM_WR8(REG_PCLK, 0x05);                     // After this display is visible on the LCD
	return 0;
}

void HOST_MEM_RD(uint32_t addr, void* rx, int len)
{
	uint8_t txBuf[4 + len];
	uint8_t rxBuf[4 + len];
	txBuf[0] = 0x3F & (addr >> 16);
	txBuf[1] = 0xFF & (addr >> 8);
	txBuf[2] = 0xFF & (addr);
	txBuf[3] = 0x00; //DUMMY
	SPI_WRT(txBuf, rxBuf, sizeof(txBuf));
	memcpy(rx, &rxBuf[4], len);
}

void HOST_MEM_WR(uint32_t addr, void* tx, int len)
{
	uint8_t txBuf[3 + len];
	txBuf[0] = (0x3F & (addr >> 16)) | 0x80;
	txBuf[1] = 0xFF & (addr >> 8);
	txBuf[2] = 0xFF & (addr);
	memcpy(&txBuf[3], tx, len);
	SPI_WRT(txBuf, NULL, sizeof(txBuf));
}

void HOST_CMD(uint32_t cmd)
{
	uint8_t tx[3];
	if(cmd == CMD_ACTIVE)
	{
		tx[0] = 0x00;
		tx[1] = 0x00;
		tx[2] = 0x00;
	}
	else
	{
		tx[0] = (0x3F & cmd) | 0x40;
		tx[1] = 0x00;
		tx[2] = 0x00;
	}
	SPI_WRT(tx, NULL, sizeof(tx));
}

void HOST_CMD_WRITE(uint8_t CMD)
{
	HOST_CMD(CMD);
}

void HOST_MEM_WR8(uint32_t addr, uint8_t data)
{
	HOST_MEM_WR(addr, &data, 1);
}

void HOST_MEM_WR16(uint32_t addr, uint16_t data)
{
	uint8_t rx[2];
	rx[0] = data;
	rx[1] = data >> 8;
	HOST_MEM_WR(addr, rx, sizeof(rx));
}

void HOST_MEM_WR32(uint32_t addr, uint32_t data)
{
	uint8_t rx[4];
	rx[0] = data;
	rx[1] = data >> 8;
	rx[2] = data >> 16;
	rx[3] = data >> 24;
	HOST_MEM_WR(addr, rx, sizeof(rx));
}

uint8_t HOST_MEM_RD8(uint32_t addr)
{
	uint8_t rx[1];
	HOST_MEM_RD(addr, rx, sizeof(rx));
	return rx[0];
}

uint16_t HOST_MEM_RD16(uint32_t addr)
{
	uint8_t rx[2];
	HOST_MEM_RD(addr, rx, sizeof(rx));
	return rx[0] | rx[1]<<8;
}


uint32_t HOST_MEM_RD32(uint32_t addr)
{
	uint8_t rx[4];
	HOST_MEM_RD(addr, rx, sizeof(rx));
	return rx[0] | rx[1]<<8 | rx[2]<<16 | rx[3]<<24;
}




/*** CMD Functions *****************************************************************/
uint8_t cmd_execute(uint32_t data)
{
	uint32_t cmdBufferRd = 0;
    uint32_t cmdBufferWr = 0;
    
    cmdBufferRd = HOST_MEM_RD32(REG_CMD_READ);
    cmdBufferWr = HOST_MEM_RD32(REG_CMD_WRITE);
    
    uint32_t cmdBufferDiff = cmdBufferWr-cmdBufferRd;
    
    if( (4096-cmdBufferDiff) > 4)
    {
        HOST_MEM_WR32(RAM_CMD + cmdBufferWr, data);
        HOST_MEM_WR32(REG_CMD_WRITE, cmdBufferWr + 4);
        return 1;
    }
    return 0;
}

uint8_t cmd(uint32_t data)
{
	uint8_t tryCount = 255;
	for(tryCount = 255; tryCount > 0; --tryCount)
	{
		if(cmd_execute(data)) { return 1; }
	}
	return 0;
}

uint8_t cmd_ready(void)
{
    uint32_t cmdBufferRd = HOST_MEM_RD32(REG_CMD_READ);
    uint32_t cmdBufferWr = HOST_MEM_RD32(REG_CMD_WRITE);
    
    return (cmdBufferRd == cmdBufferWr) ? 1 : 0;
}

/*** Track *************************************************************************/
void cmd_track(int16_t x, int16_t y, int16_t w, int16_t h, int16_t tag)
{
    cmd(CMD_TRACK);
    cmd( ((uint32_t)y<<16)|(x & 0xffff) );
    cmd( ((uint32_t)h<<16)|(w & 0xffff) );
    cmd( (uint32_t)tag );
}

/*** Draw Spinner ******************************************************************/
void cmd_spinner(int16_t x, int16_t y, uint16_t style, uint16_t scale)
{    
    cmd(CMD_SPINNER);
    cmd( ((uint32_t)y<<16)|(x & 0xffff) );
    cmd( ((uint32_t)scale<<16)|style );
    
}

/*** Draw Slider *******************************************************************/
void cmd_slider(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t options, uint16_t val, uint16_t range)
{
	cmd(CMD_SLIDER);
	cmd( ((uint32_t)y<<16)|(x & 0xffff) );
	cmd( ((uint32_t)h<<16)|(w & 0xffff) );
	cmd( ((uint32_t)val<<16)|(options & 0xffff) );
	cmd( (uint32_t)range );
}

/*** Draw Text *********************************************************************/
void cmd_text(int16_t x, int16_t y, int16_t font, uint16_t options, const char* str)
{
	/* 	
		i: data pointer
		q: str  pointer
		j: loop counter
	*/
	
	uint16_t i,j,q;
	const uint16_t length = strlen(str);
	if(!length) return ;	
	
	uint32_t* data = (uint32_t*) calloc((length/4)+1, sizeof(uint32_t));
	
	q = 0;
	for(i=0; i<(length/4); ++i, q=q+4)
	{
		data[i] = (uint32_t)str[q+3]<<24 | (uint32_t)str[q+2]<<16 | (uint32_t)str[q+1]<<8 | (uint32_t)str[q];
	}
	for(j=0; j<(length%4); ++j, ++q)
	{
		data[i] |= (uint32_t)str[q] << (j*8);
	}
	
	cmd(CMD_TEXT);
	cmd( ((uint32_t)y<<16)|(x & 0xffff) );
    cmd( ((uint32_t)options<<16)|(font & 0xffff) );
	for(j=0; j<(length/4)+1; ++j)
	{
		cmd(data[j]);
	}
	free(data);
}

/*** Draw Button *******************************************************************/
void cmd_button(int16_t x, int16_t y, int16_t w, int16_t h, int16_t font, uint16_t options, const char* str)
{	
	/* 	
		i: data pointer
		q: str  pointer
		j: loop counter
	*/
	
	uint16_t i,j,q;
	const uint16_t length = strlen(str);
	if(!length) return ;
	
	uint32_t* data = (uint32_t*) calloc((length/4)+1, sizeof(uint32_t));
	
	q = 0;
	for(i=0; i<(length/4); ++i, q=q+4)
	{
		data[i] = (uint32_t)str[q+3]<<24 | (uint32_t)str[q+2]<<16 | (uint32_t)str[q+1]<<8 | (uint32_t)str[q];
	}
	for(j=0; j<(length%4); ++j, ++q)
	{
		data[i] |= (uint32_t)str[q] << (j*8);
	}
	
	cmd(CMD_BUTTON);
	cmd( ((uint32_t)y<<16)|(x & 0xffff) );
	cmd( ((uint32_t)h<<16)|(w & 0xffff) );
    cmd( ((uint32_t)options<<16)|(font & 0xffff) );
	for(j=0; j<(length/4)+1; ++j)
	{
		cmd(data[j]);
	}
	free(data);
}

/*** Draw Keyboard *****************************************************************/
void cmd_keys(int16_t x, int16_t y, int16_t w, int16_t h, int16_t font, uint16_t options, const char* str)
{
	/* 	
		i: data pointer
		q: str  pointer
		j: loop counter
	*/
	
	uint16_t i,j,q;
	const uint16_t length = strlen(str);
	if(!length) return ;
	
	uint32_t* data = (uint32_t*) calloc((length/4)+1, sizeof(uint32_t));
	
	q = 0;
	for(i=0; i<(length/4); ++i, q=q+4)
	{
		data[i] = (uint32_t)str[q+3]<<24 | (uint32_t)str[q+2]<<16 | (uint32_t)str[q+1]<<8 | (uint32_t)str[q];
	}
	for(j=0; j<(length%4); ++j, ++q)
	{
		data[i] |= (uint32_t)str[q] << (j*8);
	}
	
	cmd(CMD_KEYS);
	cmd( ((uint32_t)y<<16)|(x & 0xffff) );
	cmd( ((uint32_t)h<<16)|(w & 0xffff) );
    cmd( ((uint32_t)options<<16)|(font & 0xffff) );
	for(j=0; j<(length/4)+1; ++j)
	{
		cmd(data[j]);
	}
	free(data);
}

/*** Write zero to a block of memory ***********************************************/
void cmd_memzero(uint32_t ptr, uint32_t num)
{
	cmd(CMD_MEMZERO);
	cmd(ptr);
	cmd(num);
}

/*** Set FG color ******************************************************************/
void cmd_fgcolor(uint32_t c)
{
	cmd(CMD_FGCOLOR);
	cmd(c);
}

/*** Set BG color ******************************************************************/
void cmd_bgcolor(uint32_t c)
{
	cmd(CMD_BGCOLOR);
	cmd(c);
}

/*** Set Gradient color ************************************************************/
void cmd_gradcolor(uint32_t c)
{
	cmd(CMD_GRADCOLOR);
	cmd(c);
}

/*** Draw Gradient *****************************************************************/
void cmd_gradient(int16_t x0, int16_t y0, uint32_t rgb0, int16_t x1, int16_t y1, uint32_t rgb1)
{
	cmd(CMD_GRADIENT);
	cmd( ((uint32_t)y0<<16)|(x0 & 0xffff) );
	cmd(rgb0);
	cmd( ((uint32_t)y1<<16)|(x1 & 0xffff) );
	cmd(rgb1);
}

/*** Matrix Functions **************************************************************/
void cmd_loadidentity(void)
{
	cmd(CMD_LOADIDENTITY);
}

void cmd_setmatrix(void)
{
	cmd(CMD_SETMATRIX);
}

void cmd_rotate(int32_t angle)
{
	cmd(CMD_ROTATE);
	cmd(angle);
}

void cmd_translate(int32_t tx, int32_t ty)
{
	cmd(CMD_TRANSLATE);
	cmd(tx);
	cmd(ty);
}
