#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#define SizeOfRacket 10

/*
* Gray = 1 / 50p
* White = 2 / 60p
* Orange = 3 / 60p
* Blue = 4 / 70p
* Green = 5 / 80p
* Red = 6 / 90p
* Yellow = 7 /120p
* Purple = 8 /no points can't be destoryed.
*/

extern SYSCALL sleept(int);
extern struct intmap far* sys_imp;
int receiver_pid, point_in_cycle, gcycle_length, gno_of_pids, front = -1, rear = -1; // existing varibles
int sched_arr_pid[5] = { -1 }, sched_arr_int[5] = { -1 };                            // existing arrays
int BallOnRacket = 1;                                                                // flags
char display_draft[25][160];
volatile int RacketPosition, BallPosition;
unsigned char far* b800h;
char display[4001], ch_arr[2048];

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

typedef struct position
{
	int x;
	int y;

} POSITION;

void DrawBall()
{
	display_draft[23][BallPosition] = 233;//'o';
	display_draft[23][BallPosition + 1] = 4; // red
}

void RemoveBall()
{
	b800h[BallPosition] = ' ';
	b800h[BallPosition + 1] = 0; // black
}

void DrawRacket() /* drawing the racket on the screen */
{
	int i;
	for (i = 0; i < SizeOfRacket; i += 2)
	{
		display_draft[24][RacketPosition + i] = 220;
		display_draft[24][RacketPosition + i + 1] = 112;
	}
	//if (BallOnRacket)
	//DrawBall();
}

void RemoveRacket() /* removing the parts of the racket and the ball */
{
	int i;
	if (BallOnRacket)
		RemoveBall();
	for (i = 0; i < SizeOfRacket; i += 2) // deleting the racket
	{
		b800h[RacketPosition + i] = ' ';
		b800h[RacketPosition + i + 1] = 0;
	}
}

void MoveBallUp()
{
	// kprintf("shit");
	BallOnRacket = 0;
	while (b800h[BallPosition + 1 - 160] == 0) // going through black screen without bricks
	{
		RemoveBall();
		BallPosition -= 160;
		kprintf("%d", BallPosition);
		DrawBall();
		// sleep(1);
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
			result = 'a';
		}
		else if (scan == 72)
		{
			result = 'w';
		}
		else if (scan == 77)
		{
			result = 'd';
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
			if (((ch == 'a') || (ch == 'A')) && RacketPosition > 2)
			{
				display_draft[24][RacketPosition + 8] = ' ';
				display_draft[24][RacketPosition + 8 + 1] = 0;
				RacketPosition -= 2;
			}
			else if (((ch == 'd') || (ch == 'D')) && RacketPosition < 90)
			{
				display_draft[24][RacketPosition] = ' ';
				display_draft[24][RacketPosition + 1] = 0;
				RacketPosition += 2;
			}
			/*
			else if (ch == ' ')
			if (no_of_arrows < ARROW_NUMBER)
			{
			arrow_pos[no_of_arrows].x = gun_position;
			arrow_pos[no_of_arrows].y = 23;
			no_of_arrows++;	*/

		} // if							
		DrawRacket();
		for (i = 0; i < 25; i++)
			for (j = 0; j < 160; j++)
				display[i * 160 + j] = display_draft[i][j];
		display[4000] = '\0';
	} // while(front != -1)
}

void frameDraw(int lvlDrawerPID) //draw the frame for the game
{
	int i, j;
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

void xmain()
{
	int lvl1matrix[25][80] = { 0 };
	int i, j, space = 0, uppid, dispid, recvpid, frameDrawPID, lvlDrawerPID;
	char *arkanoid = "Arkanoid", *scoreLabel = "Score:";
	RacketPosition = 46;//3886;
	BallPosition = 50;//3730;
	b800h = (unsigned char far*)0xB8000000;
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
	schedule(3, 10, dispid, 0, uppid, 5, frameDrawPID, 0);
	//sleep(10);
	/*
	asm {
	PUSH AX
	MOV AX, 2
	INT 10h
	POP AX
	}*/
} // xmain