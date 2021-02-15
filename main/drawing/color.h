/*
 * color.h
 *
 *  Created on: Feb 15, 2021
 *      Author: Bas
 */

#ifndef MAIN_DRAWING_COLOR_H_
#define MAIN_DRAWING_COLOR_H_


#include <stdint.h>



class Color
{
public:
	uint8_t R = 0;
	uint8_t G = 0;
	uint8_t B = 0;

	Color()
	{

	}

	Color(uint8_t R, uint8_t G, uint8_t B)
	{
		this->R = R;
		this->G = G;
		this->B = B;
	}

	uint16_t Get_RGB565()
	{
		return (((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
	}
};




#endif /* MAIN_DRAWING_COLOR_H_ */
