/*
 * FT800.h
 *
 *  Created on: Feb 14, 2021
 *      Author: Bas
 */

#ifndef MAIN_FT800_H_
#define MAIN_FT800_H_

#include <vector>
#include "../../components/freertos_cpp/freertos.h"
#include "../controls/control.h"
#include "../drawing/graphics.h"
#include "ft800Lib/ft800lib.h"


class FT800 : public Graphics
{
	enum class Notifications
	{
		Refresh	= 1,
		ScreenCalibrate,
	};
	FreeRTOS::NotifyableTask *task = NULL;
	FreeRTOS::RecursiveMutex controlsMutex;
	std::vector<Control*> controls;
	int touchPollTime = 10;
	Rectangle window = Rectangle(0, 0, 480, 272);

	float xScale 	= 0.493033;
	float xOffset 	= -16.623795;
	float yScale 	= -0.294811;
	float yOffset 	= 281.521210;

	//void Dot(int x, int y);
	//void CaliDot(int dx, int dy, int *tx, int *ty);
	//void Calibrate();
	void Work(void *arg);

public:
	FT800();

	void DrawRectangle(Pen pen, Rectangle rect);
	void DrawLine(Pen pen, int x1, int y1, int x2, int y2);
	void AddControl(Control *ctrl);
	void Refresh();
	void Begin();
	void End();

};


#endif /* MAIN_FT800_H_ */
