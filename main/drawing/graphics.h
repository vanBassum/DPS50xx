/*
 * graphics.h
 *
 *  Created on: Feb 14, 2021
 *      Author: Bas
 */

#ifndef MAIN_DRAWING_GRAPHICS_H_
#define MAIN_DRAWING_GRAPHICS_H_

#include "pen.h"
#include "rectangle.h"

class Graphics
{
public:

	virtual ~Graphics(){};
	virtual void DrawRectangle(Pen pen, Rectangle rect) = 0;
	virtual void Begin() = 0;
	virtual void End() = 0;

};


#endif /* MAIN_DRAWING_GRAPHICS_H_ */
