/*
 * pen.h
 *
 *  Created on: Feb 15, 2021
 *      Author: Bas
 */

#ifndef MAIN_DRAWING_PEN_H_
#define MAIN_DRAWING_PEN_H_

#include "color.h"

class Pen
{
public:
	Color color;
	int Width = 1;

	Pen(){}
	Pen(uint8_t r, uint8_t g, uint8_t b)
	{
		color = Color(r, g, b);
	}
};



#endif /* MAIN_DRAWING_PEN_H_ */
