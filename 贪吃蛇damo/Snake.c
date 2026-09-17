#include "Snake.h"

int S = 400;

void Init_Snake() {
	Snake.size = 2;
	Snake.body[0].X = W / 2;
	Snake.body[0].Y = H / 2;
	Snake.body[1].X = W / 2 - 1;
	Snake.body[1].Y = H / 2;
}

void Init_Food() {
	{
		int isOverlap;
		do {
			isOverlap = 0;
			Food.X = rand() % W;
			Food.Y = rand() % H;

			for (int i = 0; i < Snake.size; i++) {
				if (Food.X == Snake.body[i].X && Food.Y == Snake.body[i].Y) {
					isOverlap = 1;
					break;
				}
			}
		} while (isOverlap);
	}
}

void Init_Wall() {
	for (int i = 0; i <= H; i++) {
		for (int j = 0; j <= W; j++) {
			if (j == W) {
				printf("|");
			}
			else if (i == H) {
				printf("_");
			}
			else {
				printf(" ");
			}
		}
		printf("\n");
	}
}

void Init_UI() {
	COORD coord = { 0 };
	for (int i = 0; i < Snake.size; i++) {
		coord.X = Snake.body[i].X;
		coord.Y = Snake.body[i].Y;
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
		if (i == 0) {
			putchar('@');
		}
		else {
			putchar('*');
		}
	}
	coord.X = tailX;
	coord.Y = tailY;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	putchar(' ');

	coord.X = Food.X;
	coord.Y = Food.Y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	putchar('#');
}

void PlayGame() {
	char key = 'd';
	while (Snake.body[0].X >= 0 && Snake.body[0].X < W && Snake.body[0].Y >= 0 && Snake.body[0].Y < H) {
		Init_UI();
		if (_kbhit()) {
			key = _getch();
		}
		switch (key) {
		case 'w':kx = 0; ky = -1; break;
		case 's':kx = 0; ky = +1; break;
		case 'a':kx = -1; ky = 0; break;
		case 'd':kx = +1; ky = 0; break;
		default: break;
		}
		for (int i = 1; i < Snake.size; i++) {
			if (Snake.body[0].X == Snake.body[i].X && Snake.body[0].Y == Snake.body[i].Y) {
				return;
			}
		}

		if (Snake.body[0].X == Food.X && Snake.body[0].Y == Food.Y) {
			Init_Food();
			Snake.size++;
			score += 5;
			if (score == 30)S -= 10;
		}

		// 蛇身移动（从尾巴往头遍历）
		// 如果是吃食物，先把尾巴保存下来，作为新增节点的坐标
		tailX = Snake.body[Snake.size - 1].X;
		tailY = Snake.body[Snake.size - 1].Y;

		for (int i = Snake.size - 1; i > 0; i--) {
			Snake.body[i].X = Snake.body[i - 1].X;
			Snake.body[i].Y = Snake.body[i - 1].Y;
		}
		Snake.body[0].X += kx;
		Snake.body[0].Y += ky;

		Sleep(S);
	}
	return;
}

void ShowScore() {
	COORD coord;
	coord.X = 0;
	coord.Y = H + 2;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	printf("GAME OVER!\n");
	printf("成绩:%d", score);
}

void main() {
	CONSOLE_CURSOR_INFO cci;
	cci.bVisible = FALSE;
	cci.dwSize = sizeof(cci);
	SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cci);

	srand((unsigned int)time(NULL));
	Init_Snake();
	Init_Food();
	Init_Wall();
	Init_UI();

	PlayGame();
	ShowScore();

	system("pause");
	return 0;
}