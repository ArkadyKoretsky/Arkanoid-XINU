#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>

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

typedef struct position
{
	int x;
	int y;
} POSITION;

typedef struct brick
{
	char enbale;
	int x;
	int y;
	int score;
	int hits;
} Brick;

extern SYSCALL sleept(int);
extern struct intmap far* sys_imp;
int receiver_pid, point_in_cycle, gcycle_length, gno_of_pids, front = -1, rear = -1; // existing varibles
int sched_arr_pid[5] = { -1 }, sched_arr_int[5] = { -1 };                            // existing arrays
int BallOnRacket, MoveRightUp, MoveLeftUp, MoveRightDown, MoveLeftDown;                                    // flags
char display_draft[25][160];
volatile int RacketPosition, PositionOfTheLastLife;
unsigned char far* b800h;
char display[4001], ch_arr[2048];
int lifeCounter = 3;
POSITION BallPosition;

enum color
{
	Gray = 112,
	White = 240,
	Orange = 192,
	Blue = 16,
	Green = 32,
	Red = 64,
	Yellow = 224,
	Purple = 80
};

/*
b800h[1112] = (RacketPosition / 10) + '0';
b800h[1113] = 32;
b800h[1114] = RacketPosition % 10 + '0';
b800h[1115] = 32;
*/

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
			MoveLeftUp = 1;
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
			MoveRightUp = 1;
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
			MoveRightDown = 1;
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
			MoveLeftDown = 1;
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
	int i;
	while (1)
	{
		receive();
		// sleept(18);
		// printf(display);
		for (i = 0; i < 4000; i += 2)
		{
			b800h[i] = display[i];
			b800h[i + 1] = display[i + 1];
		}
		if (MoveRightUp)
			moveBallUpRight();
		else if (MoveLeftUp)
			moveBallUpLeft();
		else if (MoveRightDown)
			moveBallDownRight();
		else if (MoveLeftDown)
			moveBallDownLeft();
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
	/*
	for (i = 592, j = 0; i < 604; j++, i += 2) // draw score
	{
		b800h[i] = scoreLabel[j];
		b800h[i + 1] = White;
	}			  */
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


void lvlDrawer()  //draw the first level
{
	int i, j, space = 0, msg = receive();
	if (msg == 1) //receive from frameDraw
	{
		for (i = 0; i < 25; i++)
		{
			for (j = 0; j < 160; j += 2)
			{
				if (space == 0)
				{
					if (j > 1 && j < 100) {
						if (i == 3)
						{
							display_draft[i][j + 1] = White;
						}
						else if (i == 4)
						{
							display_draft[i][j + 1] = Red;
						}
						else if (i == 5)
						{
							display_draft[i][j + 1] = Yellow;
						}
						else if (i == 6)
						{
							display_draft[i][j + 1] = Blue;
						}
						else if (i == 7)
						{
							display_draft[i][j + 1] = Green;
						}
						space = 1;
					}
				}
				else
				{
					space = 0;
				}
			}
		}
		DrawRacket();
		DrawBall();
	}
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
	BallPosition.x = 50; // 3730
	BallPosition.y = 23;
	RacketPosition = 46; // 3886
	b800h = (unsigned char far*)0xB8000000;
	BallOnRacket = 1;
	MoveRightDown = MoveLeftDown = MoveLeftUp = MoveRightUp = 0;
	PositionOfTheLastLife = 134;
}

void xmain()
{
	int lvl1matrix[25][80] = { 0 };
	int i, j, uppid, dispid, recvpid, frameDrawPID, lvlDrawerPID;
	InitializeGlobalVariables();
	/*
	asm{
	PUSH AX
	XOR AH,AH
	MOV AL,3
	INT 10h
	POP AX
	} */
	/*

	/*
	DrawRacket();
	for (i = 286, j = 0; i < 302; j++, i += 2)				// draw arkanoid
	{
	b800h[i] = arkanoid[j];
	b800h[i + 1] = White;
	}
	for (i = 592, j = 0; i < 604; j++, i += 2) // draw score
	{
	b800h[i] = scoreLabel[j];
	b800h[i + 1] = White;
	}
	for (i = 132; i < 138; i += 2) // draw life
	{
	b800h[i] = 3;
	b800h[i + 1] = 4;
	}*/
	resume(lvlDrawerPID = create(lvlDrawer, INITSTK, INITPRIO, "lvlDrawer", 0));
	resume(frameDrawPID = create(frameDraw, INITSTK, INITPRIO + 4, "FrameDraw", 1, lvlDrawerPID));
	resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
	resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
	resume(uppid = create(updater, INITSTK, INITPRIO, "UPDATER", 0));
	receiver_pid = recvpid;
	set_new_int9_newisr();
	schedule(2, 1, dispid, 0, uppid, 0, frameDrawPID, 0);
	//sleep(10);
	/*
	asm {
	PUSH AX
	MOV AX, 2
	INT 10h
	POP AX
	}*/
} // xmain