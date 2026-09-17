#include "Snake.h"

int S = 200;

void Gotoxy(int x, int y) {
    COORD c = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void Init_Snake() {
    Snake.size = 3;
    int sx = W / 2; // 30
    int sy = H / 2; // 20
    Snake.body[0].X = sx;     Snake.body[0].Y = sy;
    Snake.body[1].X = sx - 1; Snake.body[1].Y = sy;
    Snake.body[2].X = sx - 2; Snake.body[2].Y = sy;
}

void Init_Food() {
    int overlap;
    do {
        overlap = 0;
        Food.X = rand() % (W - 4) + 2; // 2 到 57，离边框更远
        Food.Y = rand() % (H - 4) + 2; // 2 到 37
        for (int i = 0; i < Snake.size; i++) {
            if (Food.X == Snake.body[i].X && Food.Y == Snake.body[i].Y) {
                overlap = 1; break;
            }
        }
    } while (overlap);
}

// 只画一次：边框 + 初始蛇 + 食物
void DrawStatic() {
    system("cls");

    // 上边框：用 _ 
    for (int x = 0; x < W + 2; x++) { Gotoxy(x, 0); putchar('_'); }
    // 下边框：用 _
    for (int x = 0; x < W + 2; x++) { Gotoxy(x, H + 1); putchar('_'); }
    // 左右边框：用 |
    for (int y = 1; y <= H; y++) {
        Gotoxy(0, y);     putchar('|');
        Gotoxy(W + 1, y); putchar('|');
    }

    // 初始蛇
    for (int i = 0; i < Snake.size; i++) {
        Gotoxy(Snake.body[i].X + 1, Snake.body[i].Y + 1);
        putchar(i == 0 ? '@' : '*');
    }

    // 食物（显眼的 #）
    Gotoxy(Food.X + 1, Food.Y + 1); putchar('#');

    // 分数
    Gotoxy(0, H + 3); printf("Score: %d", score);
}

void PlayGame() {
    char key = 'd';
    kx = 1; ky = 0;

    while (Snake.body[0].X >= 0 && Snake.body[0].X < W &&
           Snake.body[0].Y >= 0 && Snake.body[0].Y < H) {
        Sleep(S);

        if (_kbhit()) {
            key = _getch();
            switch (key) {
            case 'w': kx = 0; ky = -1; break;
            case 's': kx = 0; ky = 1; break;
            case 'a': kx = -1; ky = 0; break;
            case 'd': kx = 1; ky = 0; break;
            }
        }

        int tailX = Snake.body[Snake.size - 1].X;
        int tailY = Snake.body[Snake.size - 1].Y;

        for (int i = Snake.size - 1; i > 0; i--) {
            Snake.body[i].X = Snake.body[i - 1].X;
            Snake.body[i].Y = Snake.body[i - 1].Y;
        }

        Snake.body[0].X += kx;
        Snake.body[0].Y += ky;

        // 撞自己
        for (int i = 1; i < Snake.size; i++) {
            if (Snake.body[0].X == Snake.body[i].X &&
                Snake.body[0].Y == Snake.body[i].Y) {
                Gotoxy(0, H + 3);
                printf("Game Over! 撞自己了。得分：%d   ", score);
                return;
            }
        }

        // 吃食物
        int ateFood = 0;
        if (Snake.body[0].X == Food.X && Snake.body[0].Y == Food.Y) {
            score += 10;
            Snake.size++;
            Snake.body[Snake.size - 1].X = tailX;
            Snake.body[Snake.size - 1].Y = tailY;
            Init_Food();
            ateFood = 1;
        }

        // ===== 局部刷新（核心：只动 3 个格子） =====
        // 1. 擦掉旧尾巴
        Gotoxy(tailX + 1, tailY + 1); putchar(' ');
        // 2. 画新蛇头
        Gotoxy(Snake.body[0].X + 1, Snake.body[0].Y + 1); putchar('@');
        // 3. 原来的蛇头位置变成身体
        Gotoxy(Snake.body[1].X + 1, Snake.body[1].Y + 1); putchar('*');
        // 4. 如果吃了食物，就在新位置画一个
        Gotoxy(Food.X + 1, Food.Y + 1); putchar('#');
        // 5. 刷新分数
        Gotoxy(0, H + 3); printf("Score: %d   ", score);
    }

    Gotoxy(0, H + 3);
    printf("Game Over! 撞墙了。得分：%d   ", score);
}

int main() {
    // 窗口必须够高（H=40 + 边框 + 分数 = 43行以上）
    system("mode con cols=80 lines=50");

    CONSOLE_CURSOR_INFO cci;
    cci.bVisible = FALSE;
    cci.dwSize = sizeof(cci);
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cci);

    srand((unsigned int)time(NULL));
    Init_Snake();
    Init_Food();

    printf("按任意键开始...");
    _getch();

    DrawStatic(); // 只画一次地图和初始状态
    PlayGame();   // 循环里只用 Gotoxy 局部刷新

    system("pause");
    return 0;
}