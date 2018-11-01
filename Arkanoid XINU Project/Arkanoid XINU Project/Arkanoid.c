#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <string.h>
#include <proc.h>
#include <sleep.h>

#define SizeOfRacket 10
#define Right 8
#define Left 0

/*
* Gray = 1 / 50p
* White = 2 / 60p
* Orange = 3 / 60p
* Blue = 4 / 70p
* Green = 5 / 80p
* Red = 6 / 90p
* Yellow = 7 / 120p
* Purple = 8 / no points can't be destoryed.
*/

typedef enum { false, true } bool;

typedef enum color
{
	Gray = 7,
	White = 15,
	Orange = 13,
	Blue = 1,
	Green = 2,
	Red = 4,
	Yellow = 14,
	Purple = 5,
	Black = 0
} color;

typedef struct Position
{
	int x;
	int y;
} Position;

typedef struct brick
{
	bool enable;
	int score;
	int hits;
	color surprise;
} Brick;

extern SYSCALL sleept(int);
extern struct intmap far* sys_imp;
int receiver_pid, point_in_cycle, gcycle_length, gno_of_pids, front = -1, rear = -1; // existing varibles
int sched_arr_pid[5] = { -1 }, sched_arr_int[5] = { -1 };                            // existing arrays
volatile int BallOnRacket, MoveRightUp, MoveLeftUp, MoveRightDown, MoveLeftDown, surpriseIsDropped[10] = { 0 }; // flags
volatile unsigned int score;
volatile INTPROC  new_int9(int mdevno);
volatile int mapinit(int vec, int(*newisr)(), int mdevno), count0x70;
char display_draft[25][160];
volatile int RacketPosition, PositionOfTheLastLife, lifeCounter, surprisesIndex;
unsigned char far* b800h;
char display[4001], ch_arr[2048], old_0A1h_mask, x71h1, x71h2, x71h3, old_70h_A_mask;
Brick matrix[25][80];
Position BallPosition, surprisePosition[10] = { 0 };
color surpriseColor[10] = { 0 };

void printScore(int score)
{
	int i, j;
	char str[7];
	sprintf(str, "%d", score);
	for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(str); j++, i += 2)
	{
		display_draft[8][i] = str[j];
		display_draft[8][i + 1] = White;
	}
}

void removeSurprise(int index)
{
	if (surprisePosition[index].y > 7) // went through all the bricks
	{
		display_draft[surprisePosition[index].y][surprisePosition[index].x] = ' ';
		display_draft[surprisePosition[index].y][surprisePosition[index].x + 1] = 0;
	}
	else // still falling through the bricks
		display_draft[surprisePosition[index].y][surprisePosition[index].x] = 254;
}

void drawSurprise(int index, color surpriseColor)
{
	display_draft[surprisePosition[index].y][surprisePosition[index].x] = 1; // :) - ascii
	if (surprisePosition[index].y > 7) // went through all the bricks
		display_draft[surprisePosition[index].y][surprisePosition[index].x + 1] = surpriseColor;
}

void DrawBall()
{
	display_draft[BallPosition.y][BallPosition.x] = 233; // 'o'
	display_draft[BallPosition.y][BallPosition.x + 1] = 4; // red
}

void RemoveBall()
{
	display_draft[BallPosition.y][BallPosition.x] = ' ';
	display_draft[BallPosition.y][BallPosition.x + 1] = 0;
}

void DeleteLife()
{
	char* gameOverStr = "Game Over";
	int i, j;
	lifeCounter--;
	if (lifeCounter > 0)
	{
		display_draft[5][PositionOfTheLastLife] = ' ';
		display_draft[5][PositionOfTheLastLife + 1] = 0;
		PositionOfTheLastLife -= 2;
		RemoveBall();
		BallPosition.y = 23;
		BallPosition.x = RacketPosition + 4;
		DrawBall();
		BallOnRacket = 1;
	}
	else
	{
		display_draft[5][PositionOfTheLastLife] = ' ';
		display_draft[5][PositionOfTheLastLife + 1] = 0;
		RemoveBall();
		for (i = 0, j = 0; i < 18; i += 2, j++)
		{
			display_draft[16][42 + i] = gameOverStr[j];
			display_draft[16][42 + 1 + i] = Purple;
		}
	}
}

void DrawRacket() /* drawing the racket on the screen */
{
	int i;
	for (i = 0; i < SizeOfRacket; i += 2)
	{
		display_draft[24][RacketPosition + i] = 220;
		display_draft[24][RacketPosition + i + 1] = 112;
	}
	if (BallOnRacket)
		DrawBall();
}

void RemoveRacket(int direction) /* removing the parts of the racket and the ball */
{
	display_draft[24][RacketPosition + direction] = ' ';
	display_draft[24][RacketPosition + direction + 1] = 0;
	if (BallOnRacket)
		RemoveBall();
}

void BreakTheBrick(int i, int j)
{
	int k = 0;
	if (matrix[i][j / 2].enable == true)
	{
		if (matrix[i][j / 2].hits > 1)
		{
			matrix[i][j / 2].hits--;
		}
		else
		{
			score += matrix[i][j / 2].score;
			printScore(score);
			display_draft[i][j] = ' ';
			display_draft[i][j + 1] = 0;
			if (matrix[i][j / 2].surprise != Black)
			{
				matrix[i][j / 2].surprise = Black;
				while (surprisePosition[k].x != j || surprisePosition[k].y != i)
					k++;
				surpriseIsDropped[k] = 1;
			}
		}
	}
}

void moveBallDownLeft()
{
	if (MoveLeftDown == 1 && display_draft[BallPosition.y + 1][BallPosition.x - 1] == 0 && BallPosition.y < 24)
	{
		RemoveBall();
		BallPosition.y++;
		BallPosition.x -= 2;
		DrawBall();
	}
	else
	{
		MoveLeftDown = 0;
		if (BallPosition.x == 2) // western wall
			MoveRightDown = 1;
		else if (BallPosition.y > 23) // fell down
			DeleteLife();
		else
		{
			BreakTheBrick(BallPosition.y + 1, BallPosition.x - 2);
			MoveLeftUp = 1;
		}
	}
}

void moveBallDownRight()
{
	if (MoveRightDown == 1 && display_draft[BallPosition.y + 1][BallPosition.x + 3] == 0 && BallPosition.y < 24)
	{
		RemoveBall();
		BallPosition.y++;
		BallPosition.x += 2;
		DrawBall();
	}
	else
	{
		MoveRightDown = 0;
		if (BallPosition.x == 98) // eastern wall
			MoveLeftDown = 1;
		else if (BallPosition.y > 23) // fell down
			DeleteLife();
		else
		{
			BreakTheBrick(BallPosition.y + 1, BallPosition.x + 2);
			MoveRightUp = 1;
		}
	}
}

void moveBallUpRight()
{
	if (MoveRightUp == 1 && display_draft[BallPosition.y - 1][BallPosition.x + 3] == 0)
	{
		RemoveBall();
		BallPosition.y--;
		BallPosition.x += 2;
		DrawBall();
	}
	else
	{
		MoveRightUp = 0;
		if (BallPosition.x == 98) // eastern wall
			MoveLeftUp = 1;
		else
		{
			BreakTheBrick(BallPosition.y - 1, BallPosition.x + 2);
			MoveRightDown = 1;
		}
	}
}

void moveBallUpLeft()
{
	if (MoveLeftUp == 1 && display_draft[BallPosition.y - 1][BallPosition.x - 1] == 0)
	{
		RemoveBall();
		BallPosition.y--;
		BallPosition.x -= 2;
		DrawBall();
	}
	else
	{
		MoveLeftUp = 0;
		if (BallPosition.x == 2) // western wall
			MoveRightUp = 1;
		else
		{
			BreakTheBrick(BallPosition.y - 1, BallPosition.x - 2);
			MoveLeftDown = 1;
		}
	}
}

void ballUpdater()
{
	while (1)
	{
		if (tod % 3 == 0)
		{
			receive();
			if (MoveRightUp)
				moveBallUpRight();
			else if (MoveLeftUp)
				moveBallUpLeft();
			else if (MoveRightDown)
				moveBallDownRight();
			else if (MoveLeftDown)
				moveBallDownLeft();
		}
	}
}

INTPROC new_int9(int mdevno)
{
	char result = 0;
	int scan = 0, ascii = 0;
	asm{
		MOV AH,1
		INT 16h
		JZ Skip1
		MOV AH,0
		INT 16h
		MOV BYTE PTR scan,AH
		MOV BYTE PTR ascii,AL
	} //asm
		if (scan == 75)
		{
			result = 'l';
		}
		else if (scan == 57)
		{
			result = ' ';
		}
		else if (scan == 77)
		{
			result = 'r';
		}
	if ((scan == 46) && (ascii == 3)) // Ctrl-C?
	{
		asm INT 27; // terminate xinu
	}
	send(receiver_pid, result);
Skip1:
} // new_int9

INTPROC new_int70(int mdevno)
{
	asm{
		CLI
		PUSH AX
		IN AL,0A1h
		MOV old_0A1h_mask,AL
		AND AL,0FEh
		OUT 0A1h,AL
		IN AL,70h
		//A
		MOV AL,0Ah
		OUT 70h,AL
		MOV AL,8Ah
		OUT 70h,AL
		IN AL,71h
		MOV BYTE PTR x71h1,AL
		MOV old_70h_A_mask,AL
		AND AL,10000000b
		OR AL,16 / 2 //ints per sec
		OUT 71h,AL
		IN AL,71h
		IN AL,70h
		//B
		MOV AL,0Bh
		OUT 70h,AL
		MOV AL,8Bh
		OUT 70h,AL
		IN AL,71h
		MOV BYTE PTR x71h2,AL
		OR AL,40h
		OUT 71h,AL
		IN AL,71h

		MOV byte ptr x71h3,AL
		IN AL, 021h
		AND AL, 0FBh
		OUT 021h, AL
		IN AL, 70h
		//C
		MOV AL, 0Ch
		OUT 70h, AL
		IN AL, 70h
		MOV AL, 8Ch
		OUT 70h, AL
		IN AL, 71h
		IN AL, 70h
		//D 
		MOV AL, 0Dh
		OUT 70h, AL
		IN AL, 70h
		MOV AL, 8Dh
		OUT 70h, AL
		IN AL, 71h

		STI
		POP AX

	} // asm


	count0x70++;
}/* end 70H */

void set_new_int9_newisr()
{
	int i;
	for (i = 0; i < 32; i++)
		if (sys_imp[i].ivec == 9)
		{
			sys_imp[i].newisr = new_int9;
			return;
		}
} // set_new_int9_newisr

void receiver()
{
	while (1)
	{
		char temp;
		temp = receive();
		rear++;
		ch_arr[rear] = temp;
		if (front == -1)
			front = 0;
		// getc(CONSOLE);
	} // while

} //  receiver

void displayer(void)
{
	int i, j;
	char str[7];
	while (1)
	{
		receive();
		for (i = 0; i < 4000; i += 2)
		{
			b800h[i] = display[i];
			b800h[i + 1] = display[i + 1];
		}

		for (i = 0; i < 10; i++)
			if (surpriseIsDropped[i] && surprisePosition[i].y < 25)
			{
				removeSurprise(i);
				surprisePosition[i].y++;
				drawSurprise(i, surpriseColor[i]);
			}

		/*sprintf(str, "%d", tod);
		for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(str); j++, i += 2)
		{
			display_draft[9][i] = str[j];
			display_draft[9][i + 1] = White;
		}
		sprintf(str, "%d", count0x70);
		for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(str); j++, i += 2)
		{
			display_draft[10][i] = str[j];
			display_draft[10][i + 1] = White;
		}*/
		b800h[1090] = (BallPosition.x / 10) + '0';
		b800h[1090 + 1] = 32;
		b800h[1090 + 2] = BallPosition.x % 10 + '0';
		b800h[1090 + 3] = 32;
		b800h[1250] = (BallPosition.y / 10) + '0';
		b800h[1250 + 1] = 32;
		b800h[1250 + 2] = BallPosition.y % 10 + '0';
		b800h[1250 + 3] = 32;
	} // while
} // prntr

void updater()
{
	char ch;
	int i, j;
	while (1)
	{
		receive();
		while (front != -1)
		{
			ch = ch_arr[front];
			if (front != rear)
				front++;
			else
				front = rear = -1;
			if (((ch == 'l') || (ch == 'L')) && RacketPosition > 2)
			{
				RemoveRacket(Right);
				if (BallOnRacket)
					BallPosition.x -= 2;
				RacketPosition -= 2;
				DrawRacket();
			}
			else if (((ch == 'r') || (ch == 'R')) && RacketPosition < 90)
			{
				RemoveRacket(Left);
				if (BallOnRacket)
					BallPosition.x += 2;
				RacketPosition += 2;
				DrawRacket();
			}
			else if (ch == ' ')
			{
				if (BallOnRacket)
				{
					BallOnRacket = 0;
					MoveRightUp = 1;
				}
			}
		} // while	
		for (i = 0; i < 25; i++)
			for (j = 0; j < 160; j++)
				display[i * 160 + j] = display_draft[i][j];
		display[4000] = '\0';
	} // while(front != -1)
}

void frameDraw(int lvlDrawerPID) //draw the frame for the game
{
	int i, j;
	char *arkanoid = "Arkanoid!", *scoreLabel = "Score:", *lives = "Lives:";
	for (i = 0; i < 25; i++)
	{
		for (j = 0; j < 160; j += 2)
		{
			if (j == 0 && i == 0)  //left corner
			{
				display_draft[i][j] = 201;
				display_draft[i][j + 1] = 112;
			}
			else if (i == 0 && j < 100) //upper row
			{
				display_draft[i][j] = 205;
				display_draft[i][j + 1] = 112;
			}
			else if (j == 100 && i == 0)  //right corner
			{
				display_draft[i][j] = 187;
				display_draft[i][j + 1] = 112;
			}
			else if (j == 100 || j == 0)  //right and left sides
			{
				display_draft[i][j] = 186;
				display_draft[i][j + 1] = 112;
			}
			else
			{
				display_draft[i][j] = ' ';
				display_draft[i][j + 1] = 0;
			}
		}
	}
	for (i = 124, j = 0; i <= 140; j++, i += 2)				// draw arkanoid
	{
		display_draft[1][i] = arkanoid[j];
		display_draft[1][i + 1] = White;
	}
	for (i = 592 + 4, j = 0; i < 604 + 4; j++, i += 2) // draw score
	{
		display_draft[8][i] = scoreLabel[j];
		display_draft[8][i + 1] = White;
	}
	for (i = 126, j = 0; i <= 138; j++, i += 2) // draw life
	{
		display_draft[4][i] = lives[j];
		display_draft[4][i + 1] = White;

	}
	for (i = 130; i < 136; i += 2) // draw the hearts
	{
		display_draft[5][i] = 3;
		display_draft[5][i + 1] = 4;
	}
	send(lvlDrawerPID, 1); //send msg to lvl1drawder
}

void updateBrickMatrix(int i, int j, bool state, int hits, int score)
{
	matrix[i][j].enable = state;
	matrix[i][j].hits = hits;
	matrix[i][j].score = score;
	matrix[i][j].surprise = Black;
}

void updateSurprises(int i, int j, color color)
{
	matrix[i][j].surprise = color;
	surpriseColor[surprisesIndex] = color;
	surprisePosition[surprisesIndex].x = j * 2;
	surprisePosition[surprisesIndex++].y = i;
}

void lvlDrawer()  //draw the first level
{
	int i, j, space = 0, msg = receive();
	if (msg == 1) //receive from frameDraw
	{
		for (i = 0; i < 25; i++)
		{
			for (j = 0; j < 160; j += 2)
			{
				if (j > 20 && j < 80 && i > 0)
				{
					display_draft[i][j] = 254;
					if (i == 3)
					{
						display_draft[i][j + 1] = Gray;
						updateBrickMatrix(i, j / 2, true, 1, 60);
					}
					else if (i == 4)
					{
						display_draft[i][j + 1] = Red;
						updateBrickMatrix(i, j / 2, true, 2, 90);
						switch (j % 29)
						{
						case 1:
							updateSurprises(i, j / 2, Green);
							break;
						case 11:
							updateSurprises(i, j / 2, Orange);
							break;
						case 21:
							updateSurprises(i, j / 2, Blue);
							break;
						}
					}
					else if (i == 5)
					{
						display_draft[i][j + 1] = Yellow;
						updateBrickMatrix(i, j / 2, true, 1, 120);
						switch (j % 29)
						{
						case 1:
							updateSurprises(i, j / 2, Blue);
							break;
						case 11:
							updateSurprises(i, j / 2, Green);
							break;
						case 21:
							updateSurprises(i, j / 2, Orange);
							break;
						}
					}
					else if (i == 6)
					{
						display_draft[i][j + 1] = Blue;
						updateBrickMatrix(i, j / 2, true, 2, 70);
						switch (j % 29)
						{
						case 1:
							updateSurprises(i, j / 2, Orange);
							break;
						case 11:
							updateSurprises(i, j / 2, Blue);
							break;
						case 21:
							updateSurprises(i, j / 2, Green);
							break;
						}
					}
					else if (i == 7)
					{
						display_draft[i][j + 1] = Green;
						updateBrickMatrix(i, j / 2, true, 1, 80);
					}
				}
			}
		}
	}
	DrawRacket();
	DrawBall();
}

SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...)
{
	int i, ps, *iptr;
	disable(ps);
	gcycle_length = cycle_length;
	point_in_cycle = 0;
	gno_of_pids = no_of_pids;
	iptr = &pid1;
	for (i = 0; i < no_of_pids; i++)
	{
		sched_arr_pid[i] = *iptr;
		iptr++;
		sched_arr_int[i] = *iptr;
		iptr++;
	} // for
	restore(ps);
} // schedule

void InitializeGlobalVariables()
{
	int i;
	asm{
		PUSH AX
		PUSH DX
		MOV DX,3D4h
		MOV AL,11
		OUT DX,AX
		POP DX
		POP AX
	}
	score = 0;
	BallPosition.x = 50; // 3730
	BallPosition.y = 23;
	RacketPosition = 46; // 3886
	b800h = (unsigned char far*)0xB8000000;
	BallOnRacket = 1;
	MoveRightDown = MoveLeftDown = MoveLeftUp = MoveRightUp = 0;
	PositionOfTheLastLife = 134;
	lifeCounter = 3;
	surprisesIndex = 0;
}

void initBrick()
{
	int i, j;
	for (i = 0; i < 25; i++)
	{
		for (j = 0; j < 80; j++)
		{
			matrix[i][j].enable = false;
			matrix[i][j].hits = -1;
			matrix[i][j].score = -1;
		}
	}
}

void xmain()
{
	int lvl1matrix[25][80] = { 0 };
	int i, j, uppid, dispid, recvpid, frameDrawPID, lvlDrawerPID, ballPID;
	count0x70 = 0;
	InitializeGlobalVariables();
	initBrick();
	mapinit(112, new_int70, 122);
	resume(lvlDrawerPID = create(lvlDrawer, INITSTK, INITPRIO, "lvlDrawer", 0));
	resume(frameDrawPID = create(frameDraw, INITSTK, INITPRIO + 4, "FrameDraw", 1, lvlDrawerPID));
	resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
	resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
	resume(uppid = create(updater, INITSTK, INITPRIO, "UPDATER", 0));
	resume(ballPID = create(ballUpdater, INITSTK, INITPRIO, "BallUpdater", 0));
	receiver_pid = recvpid;
	set_new_int9_newisr();
	schedule(4, 1, dispid, 0, uppid, 0, frameDrawPID, 0, ballPID, 0);
	//sleep(10);
	/*
	asm {
	PUSH AX
	MOV AX, 2
	INT 10h
	POP AX
	}*/
} // xmain