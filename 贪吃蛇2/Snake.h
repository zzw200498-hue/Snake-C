#pragma once
#ifndef __SNAKE_H__
#define __SNAKE_H__

#include <stdio.h>
#include <conio.h> 
#include <Windows.h>
#include <time.h>
#include <stdlib.h>

// 极度精简的地图，保证一定能在默认窗口中放下
#define W 60
#define H 40

int score=0;
int kx, ky;
int tailX = 0, tailY = 0;

struct BODY {
	int X;
	int Y;
};

struct SNAKE {
	struct BODY body  [W * H];
	int size;
}Snake;

struct FOOD {
	int X;
	int Y;
}Food;

void Init_Food();
void Init_Snake();
void Init_Wall();
void Init_UI();
void PlayGame();
void ShowScore();
#endif