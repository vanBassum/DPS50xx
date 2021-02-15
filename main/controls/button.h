/*
 * button.h
 *
 *  Created on: Feb 15, 2021
 *      Author: Bas
 */

#ifndef MAIN_CONTROLS_BUTTON_H_
#define MAIN_CONTROLS_BUTTON_H_

#include "control.h"


class Button : public Control
{
public:
	int X = 0;
	int Y = 0;
	int Width = 50;
	int Height = 30;

	void Paint(Graphics* g, Rectangle window)
	{
		Rectangle dest = Rectangle(X + window.X, Y + window.Y, Width, Height);

		Pen p;
		p.color = Color(255, 0, 0);
		g->DrawRectangle(p, dest);
	}

	bool Collides(int x, int y)
	{
		bool collision = true;
		collision &= x > X && x < X + Width;
		collision &= y > Y && y < Y + Height;
		return collision;
	}
};



#endif /* MAIN_CONTROLS_BUTTON_H_ */
