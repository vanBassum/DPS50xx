/*
 * rectangle.h
 *
 *  Created on: Feb 15, 2021
 *      Author: Bas
 */

#ifndef MAIN_DRAWING_RECTANGLE_H_
#define MAIN_DRAWING_RECTANGLE_H_


class Rectangle
{
public:
	int X = 0;
	int Y = 0;
	int Width = 0;
	int Height = 0;

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


#endif /* MAIN_DRAWING_RECTANGLE_H_ */
