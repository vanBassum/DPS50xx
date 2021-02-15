/*
 * control.h
 *
 *  Created on: Feb 14, 2021
 *      Author: Bas
 */

#ifndef MAIN_CONTROLS_CONTROL_H_
#define MAIN_CONTROLS_CONTROL_H_

#include "../../components/misc/event.h"
#include "../drawing/graphics.h"
#include "../drawing/rectangle.h"


class Control
{
public:
	Event<Control*> TouchDown;
	Event<Control*> TouchUp;

	virtual ~Control(){}
	virtual void Paint(Graphics* g, Rectangle window) = 0;
	virtual bool Collides(int x, int y) = 0;

};



#endif /* MAIN_CONTROLS_CONTROL_H_ */
