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
#define ON (1)
#define OFF (0)

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
	Orange = 6,
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
int receiver_pid, point_in_cycle, gcycle_length, gno_of_pids, front = -1, rear = -1, lvl2DrawerPID, lvl3DrawerPID; // existing varibles
int sched_arr_pid[5] = { -1 }, sched_arr_int[5] = { -1 };                            // existing arrays
volatile int ballSpeed, perSpeed, BallOnRacket, MoveRightUp, MoveLeftUp, MoveRightDown, MoveLeftDown, surpriseIsDropped[10] = { 0 }; // flags
volatile unsigned int score, mytod;
volatile INTPROC  new_int9(int mdevno);
volatile char display_draft[25][160];
volatile int RacketPosition, PositionOfTheLastLife, lifeCounter, surprisesIndex;
unsigned char far* b800h;
volatile int hertz, hertzArr[4] = { 1000, 1200, 1100, 1000 };
char display[4001], ch_arr[2048], old_021h_mask, old_0A1h_mask, old_70h_A_mask;
volatile int changeLevel1Flag, changeLevel2Flag, level;
char scoreStr[7];
Brick matrix[25][80];
Position BallPosition, surprisePosition[10] = { 0 };
color surpriseColor[10] = { 0 };

void interrupt myTimerISR(void)
{
	char ui_flag, pi_flag, temp;
	asm{
		CLI
		PUSH AX

		// read c register
		MOV AL, 0CH		// ask for register c
		OUT 70H, AL
		MOV AL, 8CH
		OUT 70H, AL
		IN 	AL, 71H

		MOV AH, AL
		AND AL, 00010000B
		AND AH, 01000000B
		MOV ui_flag, AL
		MOV pi_flag, AH

		POP AX
	}

		if (pi_flag != 0)
		{
			++mytod;
		}
	asm{
		PUSH AX
		// SEND EOI
		MOV AL, 20H
		OUT 0A0H, AL
		OUT 020H, AL
		POP AX
		STI
	}
}

void setInt70h()
{
	char temp;
	setvect(0x70, myTimerISR);

	temp = 1024;

	asm{
		CLI
		PUSH AX

		// BACKUP 0A1H MASK
		IN 	AL, 0A1H
		MOV old_0A1h_mask, AL

		// BACKUP 021H MASK
		IN 	AL,021H
		MOV old_021h_mask, AL

		// SEND NEW MASK TO 0A1H
		AND AL, 0FEH
		OUT 0A1H, AL

		// SEND NEW MASK TO 021H
		AND AL,0FBH
		OUT 021H,AL

		// SET NEW BASE AND RATE (STATUS REGISTER A)
		//-----------------------------------------------------------------
		MOV AL, 0AH			// ASK FOR STATUS REGISTER A
		OUT 70H, AL
		MOV AL, 8AH			// RESEND REQUEST WITH MSB=1 (SIGNAL WRITE)
		OUT 70H, AL
		IN 	AL, 71H			// READ REGISTER A FROM PORT 71
		AND AL, 10000000B	// KEEP UIP
							//OR 	AL, 00010011B
		OR 	AL, temp
		OUT 71H, AL			// SEND NEW A REGISTER
		IN 	AL, 71H			// CONFIRM WRITE BY READ
							//-----------------------------------------------------------------

							// ACTIVATE PERIODIC INTERRUPT (STATUS REGISTER B)
							//-----------------------------------------------------------------
		MOV AL, 0BH			// ASK FOR STATUS REGISTER B
		OUT 70H, AL
		MOV AL, 8BH 		// RESEND REQUEST WITH MSB=1 (SIGNAL WRITE)
		OUT 70H, AL
		IN 	AL, 71H			// READ REGISTER B FROM PORT 71
		AND AL, 10001111B
		OR 	AL, 01010000B 	// SET PI AND UI FLAG
		OUT	71H, AL			// SEND NEW B REGISTER
		IN 	AL, 71H 		// CONFIRM WRITE BY READ
							//-----------------------------------------------------------------

							// AFTER WRITE TO A & B MUST READ C TWICE AND D ONCE
							//-----------------------------------------------------------------
							// READ C (FIRST)
		MOV AL, 0CH		// ASK FOR REGISTER C
		OUT 70H, AL
		MOV AL, 8CH		// RESEND REQUEST WITH MSB=1 (SIGNAL WRITE)
		OUT 70H, AL
		IN 	AL, 71H

		// READ C (SECOND)
		MOV AL, 0CH		// ASK FOR REGISTER C
		OUT 70H, AL
		MOV AL, 8CH		// RESEND REQUEST WITH MSB=1 (SIGNAL WRITE)
		OUT 70H, AL
		IN 	AL, 71H

		// READ D
		MOV AL, 0DH		// ASK FOR REGISTER C
		OUT 70H, AL
		MOV AL, 8DH		// RESEND REQUEST WITH MSB=1 (SIGNAL WRITE)
		OUT 70H, AL
		IN 	AL, 71H
		//-----------------------------------------------------------------
		POP AX
		STI
	}
}

void ChangeSpeaker(int status)
{
	int portval;
	//   portval = inportb( 0x61 );

	portval = 0;
	asm{
		PUSH AX
		MOV AL,61h
		MOV BYTE PTR portval,AL
		POP AX
	}

		if (status == ON)
			portval |= 0x03;
		else
			portval &= ~0x03;
	// outportb( 0x61, portval );
	asm{
		PUSH AX
		MOV AX,portval
		OUT 61h,AL
		POP AX
	} // asm

} /*--ChangeSpeaker( )----------*/

void NoSound(void)
{
	ChangeSpeaker(OFF);
} /*--NoSound( )------*/

void Sound()
{
	unsigned divisor;
	asm{
		PUSH AX
	}
	divisor = 1193180L / hertz;
	//printf ("divisor is %d\n", divisor);
	ChangeSpeaker(ON);

	//        outportb( 0x43, 0xB6 );
	asm{
		PUSH AX
		MOV AL,0B6h
		OUT 43h,AL
		POP AX
	} // asm

	  //       outportb( 0x42, divisor & 0xFF ) ;
		asm{
		PUSH AX
		MOV AX,divisor
		AND AX,0FFh
		OUT 42h,AL
		POP AX
	} // asm

	  //        outportb( 0x42, divisor >> 8 ) ;

		asm{
		PUSH AX
		MOV AX,divisor
		MOV AL,AH
		OUT 42h,AL
		POP AX
	}
		asm{
		POP AX
	}
		//NoSound ();
} /*--Sound( )-----*/


void printScore(int score)
{
	int i, j;
	char nextLevelStr[30] = "Press F1 to next level";
	sprintf(scoreStr, "%d", score);
	for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(scoreStr); j++, i += 2)
	{
		display_draft[8][i] = scoreStr[j];
		display_draft[8][i + 1] = White;
	}
	if (score >= 20 && changeLevel1Flag == 1)
	{
		for (i = 590, j = 0; j < 22; j++, i += 2)
		{
			display_draft[12][i] = nextLevelStr[j];
			display_draft[12][i + 1] = Yellow;
		}
		changeLevel1Flag = 0;
		changeLevel2Flag = 1;
	}
	if (score >= 300 && changeLevel2Flag == 1)
	{
		for (i = 590, j = 0; j < 22; j++, i += 2)
		{
			display_draft[12][i] = nextLevelStr[j];
			display_draft[12][i + 1] = Yellow;
		}
		changeLevel2Flag = 0;
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
	//lifeCounter--;
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

void BlueSurprise()
{
	int i;
	for (i = 0; i < SizeOfRacket * 2; i += 2)
	{
		display_draft[24][RacketPosition + i] = 220;
		display_draft[24][RacketPosition + i + 1] = 112;
	}
	if (BallOnRacket)
		DrawBall();
}

void BreakTheBrick(int i, int j)
{
	int k = 0;
	if (matrix[i][j / 2].enable == true)
	{
		Sound();
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

void dropSur()
{
	int i;
	while (1)
	{
		for (i = 0; i < 10; i++)
		{
			if (surpriseIsDropped[i] && surprisePosition[i].y < 24)
			{
				receive();
				removeSurprise(i);
				surprisePosition[i].y++;
				if (surprisePosition[i].y < 24)
				{
					drawSurprise(i, surpriseColor[i]);
				}
				else
				{
					if (surprisePosition[i].x >= RacketPosition && surprisePosition[i].x <= RacketPosition + SizeOfRacket)
					{
						display_draft[15][50] = 1;
						display_draft[15][50 + 1] = Purple;
					}
				}
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
		NoSound();
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
		NoSound();
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
		else if (scan == 59)
		{
			result = 'f';
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

		sprintf(str, "%d", tod);
		for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(str); j++, i += 2)
		{
			display_draft[9][i] = str[j];
			display_draft[9][i + 1] = White;
		}
		sprintf(str, "%d", mytod);
		for (i = 604 + 4, j = 0; i < 604 + 16, j < strlen(str); j++, i += 2)
		{
			display_draft[10][i] = str[j];
			display_draft[10][i + 1] = White;
		}
		b800h[1090] = (BallPosition.x / 10) + '0';
		b800h[1090 + 1] = Green;
		b800h[1090 + 2] = BallPosition.x % 10 + '0';
		b800h[1090 + 3] = Green;
		b800h[1250] = (BallPosition.y / 10) + '0';
		b800h[1250 + 1] = Green;
		b800h[1250 + 2] = BallPosition.y % 10 + '0';
		b800h[1250 + 3] = Green;
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
			else if (ch == 'f')
			{
				if (level == 1)
				{
					RemoveBall();
					//BallPosition.x = 50;
					//BallPosition.y = 23;
					send(lvl2DrawerPID, 1);
					for (i = 590, j = 0; j < 22; j++, i += 2)  //removing next level message
					{
						display_draft[12][i] = ' ';
						display_draft[12][i + 1] = Black;
					}
					for (i = 0; i < 4; i++)  //next level sounds
					{
						hertz = hertzArr[i];
						Sound();
						sleep(1);
					}
					NoSound();
					hertz = 1060;
					level = 2;
				}
				else if (level == 2)
				{
					RemoveBall();
					//BallPosition.x = 50;
					//BallPosition.y = 23;
					send(lvl3DrawerPID, 1);
					for (i = 590, j = 0; j < 22; j++, i += 2)  //removing next level message
					{
						display_draft[12][i] = ' ';
						display_draft[12][i + 1] = Black;
					}
					for (i = 0; i < 4; i++)  //next level sounds
					{
						hertz = hertzArr[i];
						Sound();
						sleep(1);
					}
					NoSound();
					hertz = 1060;
					level = 3;
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

void cleanScreen()
{
	int i, j;
	for (i = 1; i < 25; i++)
	{
		for (j = 2; j < 98; j += 2)
		{
			display_draft[i][j] = ' ';
			display_draft[i][j + 1] = Black;
		}
	}
}

void lvl3Drawer()  //draw the second level
{
	int i, j, space = 0, msg = receive();
	if (msg == 1) //receive from frameDraw
	{
		cleanScreen();
		for (i = 3; i < 8; i++)
			for (j = 20; j < 80; j += 2)
			{
				display_draft[i][j] = 254;
				display_draft[i][j + 1] = White;
			}
		///////R
		for (i = 3, j = 38; i < 8; i++)
		{
			display_draft[i][j] = 254;
			display_draft[i][j + 1] = Purple;
		}
		for (j = 40; j < 50; j += 2)
		{
			display_draft[3][j] = 254;
			display_draft[3][j + 1] = Purple;
		}
		display_draft[4][48] = 254;
		display_draft[4][49] = Purple;
		display_draft[5][48] = 254;
		display_draft[5][49] = Purple;
		display_draft[5][46] = 254;
		display_draft[5][47] = Purple;
		display_draft[5][44] = 254;
		display_draft[5][45] = Purple;
		display_draft[6][46] = 254;
		display_draft[6][47] = Purple;
		display_draft[7][48] = 254;
		display_draft[7][49] = Purple;


		///////T		
		for (j = 52; j < 66; j += 2)
		{
			display_draft[3][j] = 254;
			display_draft[3][j + 1] = Purple;
		}
		for (i = 3, j = 58; i < 8; i++)
		{
			display_draft[i][j] = 254;
			display_draft[i][j + 1] = Purple;
		}


	}
	DrawRacket();
	DrawBall();
}

void lvl2Drawer()  //draw the second level
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
						display_draft[i][j + 1] = Orange;
						updateBrickMatrix(i, j / 2, true, 1, 60);
					}
					else if (i == 4)
					{
						display_draft[i][j + 1] = Green;
						updateBrickMatrix(i, j / 2, true, 2, 80);
					}
					else if (i == 5)
					{
						display_draft[i][j + 1] = Yellow;
						updateBrickMatrix(i, j / 2, true, 3, 120);
					}
					else if (i == 6)
					{
						display_draft[i][j + 1] = Gray;
						updateBrickMatrix(i, j / 2, true, 3, 50);
					}
					else if (i == 7)
					{
						display_draft[i][j + 1] = Red;
						updateBrickMatrix(i, j / 2, true, 2, 90);
					}
					else if (i == 8)
					{
						display_draft[i][j + 1] = Blue;
						updateBrickMatrix(i, j / 2, true, 1, 80);
						switch (j % 29)
						{
						case 11:
							updateSurprises(i, j / 2, Blue);
							break;
						case 15:
							updateSurprises(i, j / 2, Blue);
							break;
						case 18:
							updateSurprises(i, j / 2, Blue);
							break;
						case 19:
							updateSurprises(i, j / 2, Blue);
							break;
						case 20:
							updateSurprises(i, j / 2, Blue);
							break;
						case 21:
							updateSurprises(i, j / 2, Blue);
							break;
						case 22:
							updateSurprises(i, j / 2, Blue);
							break;
						case 23:
							updateSurprises(i, j / 2, Blue);
							break;
						case 24:
							updateSurprises(i, j / 2, Blue);
							break;
						}
					}
				}
			}
		}
	}
	DrawRacket();
	DrawBall();
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
						/*switch (j % 29)
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
						}*/
					}
					else if (i == 5)
					{
						display_draft[i][j + 1] = Yellow;
						updateBrickMatrix(i, j / 2, true, 1, 120);
						/*switch (j % 29)
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
						}*/
					}
					else if (i == 6)
					{
						display_draft[i][j + 1] = Blue;
						updateBrickMatrix(i, j / 2, true, 2, 70);
					}
					else if (i == 7)
					{
						display_draft[i][j + 1] = Green;
						updateBrickMatrix(i, j / 2, true, 1, 80);
						switch (j % 29)
						{
						case 11:
							updateSurprises(i, j / 2, Blue);
							break;
						case 15:
							updateSurprises(i, j / 2, Blue);
							break;
						case 18:
							updateSurprises(i, j / 2, Blue);
							break;
						case 19:
							updateSurprises(i, j / 2, Blue);
							break;
						case 20:
							updateSurprises(i, j / 2, Blue);
							break;
						case 21:
							updateSurprises(i, j / 2, Blue);
							break;
						case 22:
							updateSurprises(i, j / 2, Blue);
							break;
						case 23:
							updateSurprises(i, j / 2, Blue);
							break;
						case 24:
							updateSurprises(i, j / 2, Blue);
							break;
						}
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
	hertz = 1200;
	changeLevel1Flag = 1;
	changeLevel2Flag = 0;
	level = 1;
	ballSpeed = 15;
	perSpeed = 30;
	asm{
		PUSH AX
		PUSH BX
		MOV AL, 36H
		OUT 43H, AL
		MOV BX, 10846 //CHANGE COUNT
		MOV AL, BL //TRANSFER THE LSB 1ST
		OUT 40H, AL
		MOV AL, BH //TRANSFER MSB SECOND
		OUT 40H, AL
		POP BX
		POP AX
	}
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
	int i, j, uppid, dispid, recvpid, frameDrawPID, lvlDrawerPID, ballPID, dropSurPID;
	InitializeGlobalVariables();
	initBrick();
	setInt70h();
	resume(lvlDrawerPID = create(lvlDrawer, INITSTK, INITPRIO, "lvlDrawer", 0));
	resume(lvl2DrawerPID = create(lvl2Drawer, INITSTK, INITPRIO, "lvl2Drawer", 0));
	resume(lvl3DrawerPID = create(lvl3Drawer, INITSTK, INITPRIO, "lvl3Drawer", 0));
	resume(frameDrawPID = create(frameDraw, INITSTK, INITPRIO + 4, "FrameDraw", 1, lvlDrawerPID));
	resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
	resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
	resume(uppid = create(updater, INITSTK, INITPRIO, "UPDATER", 0));
	resume(ballPID = create(ballUpdater, INITSTK, INITPRIO, "BallUpdater", 0));
	resume(dropSurPID = create(dropSur, INITSTK, INITPRIO, "dropSur", 0));
	receiver_pid = recvpid;
	set_new_int9_newisr();
	schedule(5, 2, dispid, 0, uppid, 1, frameDrawPID, 1, ballPID, 1, dropSurPID, 1);
} // xmain