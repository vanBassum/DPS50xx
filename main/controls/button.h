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
	bool pressedState = false;

	void Up(Control *ctrl)
	{
		pressedState = false;
		RedrawRequest.Invoke();
	}

	void Down(Control *ctrl)
	{
		pressedState = true;
		RedrawRequest.Invoke();
	}

public:
	int X = 0;
	int Y = 0;
	int Width = 50;
	int Height = 30;



	Button()
	{
		TouchDown.Bind(this, &Button::Down);
		TouchUp.Bind(this, &Button::Up);
	}

	void Paint(Graphics* g, Rectangle window)
	{
		Rectangle dest = Rectangle(X + window.X, Y + window.Y, Width, Height);
		Pen p;
		if(pressedState)
			p.color = Color(255, 0, 0);
		else
			p.color = Color(0, 255, 0);
		g->DrawRectangle(p, dest);
	}

	bool Collides(int x, int y)
	{
		bool collision = true;
		collision &= (x > X) && (x < (X + Width)) && (y > Y) && (y < (Y + Height));


		//ESP_LOGI("Button", "Rect = %d, %d, %d, %d", X, Y, Width, Height);
		//ESP_LOGI("Button", "col = %d, %d, %d, %d", (x > X), (x < (X + Width)),  (y > Y), (y < (Y + Height)));


		return collision;
	}
};



#endif /* MAIN_CONTROLS_BUTTON_H_ */
