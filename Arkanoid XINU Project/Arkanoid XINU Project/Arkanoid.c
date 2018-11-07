#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <string.h>
#include <proc.h>
#include <sleep.h>
#include <butler.h>

#define ON (1)
#define OFF (0)
#define LevelOneTotalScore 11890
#define LevelTwoTotalScore 13630
#define amountOfBalls 3

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

typedef enum direction
{
	RightUp = 0,
	RightDown = 1,
	LeftUp = 2,
	LeftDown = 3
} direction;

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
int receiver_pid, point_in_cycle, gcycle_length, gno_of_pids, front = -1, rear = -1, lvl2DrawerPID, lvl3DrawerPID;
int sched_arr_pid[15] = { -1 }, sched_arr_int[15] = { -1 }, ballPID[amountOfBalls];
volatile int greenSurFlag, redSurFlag, left, right, sizeOfRacket, ballSpeed, perSpeed, BallOnRacket, surpriseIsDropped[10] = { 0 };
volatile unsigned int score, mytod;
volatile INTPROC  new_int9(int mdevno);
volatile char display_draft[25][160];
volatile int RacketPosition, PositionOfTheLastLife, lifeCounter, surprisesIndex, ballsCounter;
unsigned char far* b800h;
volatile int hertz, hertz1Arr[4] = { 1000, 1200, 1100, 1000 }, hertz2Arr[4] = { 100, 80, 50, 30 };
char display[4001], ch_arr[2048], old_021h_mask, old_0A1h_mask, old_70h_A_mask;
volatile int changeLevel1Flag, changeLevel2Flag, level;
volatile char scoreStr[7];
volatile int BallsAndDirections[amountOfBalls][4]; // BallsAndDirections[0][0..3] - the directions of the first (main) ball
Brick matrix[25][80];
Position BallPosition[3], surprisePosition[10] = { 0 };
color surpriseColor[10] = { 0 };
bool ballIsActive[amountOfBalls] = { false };

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

void RemoveBall(int indexOfTheBall)
{
	display_draft[BallPosition[indexOfTheBall].y][BallPosition[indexOfTheBall].x] = ' ';
	display_draft[BallPosition[indexOfTheBall].y][BallPosition[indexOfTheBall].x + 1] = 0;
}

void DrawBall(int indexOfTheBall)
{
	display_draft[BallPosition[indexOfTheBall].y][BallPosition[indexOfTheBall].x] = 233; // 'o'
	display_draft[BallPosition[indexOfTheBall].y][BallPosition[indexOfTheBall].x + 1] = 4; // red
}

void nextLevel(int levelNum)
{
	int i, j;
	/*for (i = 0; i < amountOfBalls; i++)
		if (ballIsActive[i])
			RemoveBall(i);*/
	if (levelNum == 1)
	{
		send(lvl2DrawerPID, 1);
	}
	else if (levelNum == 2)
	{
		send(lvl3DrawerPID, 1);
	}
	for (i = 590, j = 0; j < 22; j++, i += 2)  //removing next level message
	{
		display_draft[12][i] = ' ';
		display_draft[12][i + 1] = Black;
	}
	for (i = 0; i < 4; i++)  //next level sounds
	{
		hertz = hertz1Arr[i];
		Sound();
		sleep(1);
	}
	NoSound();
	hertz = 1060;
	level = levelNum + 1;
}

void checkScore()
{
	if (level == 1 && score >= LevelOneTotalScore)
	{
		nextLevel(level);
	}
	else if (level == 2 && score >= LevelOneTotalScore + LevelTwoTotalScore)
	{
		nextLevel(level);
	}
}

void printScore(int score)
{
	int i, j;
	//char nextLevelStr[30] = "Press F1 to next level";
	itoa(score, scoreStr, 10);
	//sprintf(scoreStr, "%d", score);
	for (i = 128, j = 0; j < strlen(scoreStr); j++, i += 2)
	{
		display_draft[9][i] = scoreStr[j];
		display_draft[9][i + 1] = White;
	}
	if (score >= 20 && changeLevel1Flag == 1)
	{
		/*for (i = 110, j = 0; j < strlen(nextLevelStr); j++, i += 2)
		{
			display_draft[14][i] = nextLevelStr[j];
			display_draft[14][i + 1] = Yellow;
		}*/
		changeLevel1Flag = 0;
		changeLevel2Flag = 1;
	}
	if (score >= 300 && changeLevel2Flag == 1 && level == 2)
	{
		/*for (i = 110, j = 0; j < 22; j++, i += 2)
		{
			display_draft[14][i] = nextLevelStr[j];
			display_draft[14][i + 1] = Yellow;
		}*/
		changeLevel2Flag = 0;
	}
	checkScore();
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

void RemoveRacket(int direction)
{
	int i;
	display_draft[24][RacketPosition + direction] = ' ';
	display_draft[24][RacketPosition + direction + 1] = 0;
	if (BallOnRacket)
	{
		for (i = 0; i < amountOfBalls; i++)
		{
			if (ballIsActive[i])
				RemoveBall(i);
		}
	}
}

void removeDoubleRacket()
{
	int i;
	for (i = 0; i < 10; i += 2)
	{
		display_draft[24][RacketPosition + 10 + i] = ' ';
		display_draft[24][RacketPosition + 10 + i + 1] = 0;
	}
	if (BallOnRacket)
	{
		for (i = 0; i < amountOfBalls; i++)
		{
			if (ballIsActive[i])
				RemoveBall(i);
		}
	}
}

void DrawRacket() /* drawing the racket on the screen */
{
	int i;
	for (i = 0; i < sizeOfRacket; i += 2)
	{
		display_draft[24][RacketPosition + i] = 220;
		display_draft[24][RacketPosition + i + 1] = 112;
	}
	if (BallOnRacket)
	{
		for (i = 0; i < amountOfBalls; i++)
		{
			if (ballIsActive[i])
				DrawBall(i);
		}
	}
}

void endGame()
{
	int i, j;
	char* gameOverStr = "Game Over";
	display_draft[5][PositionOfTheLastLife] = ' ';
	display_draft[5][PositionOfTheLastLife + 1] = 0;
	/*for (i = 0; i < amountOfBalls; i++)
		if (ballIsActive[i])
			RemoveBall(i);*/
	cleanScreen();
	gameOverStr = "Game Over";
	gameOverStr = "Game Over";
	for (i = 0, j = 0; i < 18; i += 2, j++)
	{
		display_draft[15][42 + i] = gameOverStr[j];
		display_draft[15][42 + 1 + i] = Purple;
	}
	for (i = 0; i < 4; i++)  //next level sounds
	{
		hertz = hertz2Arr[i];
		Sound();
		sleep(1);
	}
	NoSound();
	hertz = 1060;
}

void clearAllDirections(int indexOfTheBall)
{
	BallsAndDirections[indexOfTheBall][RightUp] = BallsAndDirections[indexOfTheBall][RightDown] = BallsAndDirections[indexOfTheBall][LeftUp] = BallsAndDirections[indexOfTheBall][LeftDown] = 0;
	if (ballsCounter == 1)
		BallOnRacket = 1;
}

void DeleteLife(int indexOfTheBall)
{
	int i, j;
	if (ballsCounter == 1)
	{
		lifeCounter--;
		if (lifeCounter > 0)
		{
			display_draft[5][PositionOfTheLastLife] = ' ';
			display_draft[5][PositionOfTheLastLife + 1] = 0;
			PositionOfTheLastLife -= 2;
			RemoveBall(indexOfTheBall);
			BallPosition[indexOfTheBall].y = 23;
			if (sizeOfRacket == 10)
			{
				BallPosition[indexOfTheBall].x = RacketPosition + (sizeOfRacket / 2) - 1;
			}
			else
			{
				BallPosition[indexOfTheBall].x = RacketPosition + (sizeOfRacket / 2);
			}
			DrawBall(indexOfTheBall);
			BallOnRacket = 1;
		}
		else
		{
			endGame();
		}
	}
	else
	{
		RemoveBall(indexOfTheBall);
		ballIsActive[indexOfTheBall] = false;
		clearAllDirections(indexOfTheBall);
		ballsCounter--;
		suspend(getpid());
	}
}

void BreakTheBrick(int i, int j, int type) //type 0 for ball and type 1 for lazer
{
	int k = 0;
	if (matrix[i][j / 2].enable == true)
	{
		Sound();
		if (matrix[i][j / 2].hits >= 1)
		{
			if (type == 1)
			{
				matrix[i][j / 2].hits--;
			}
			else
			{
				matrix[i][j / 2].hits -= 2;
			}
		}
		if (matrix[i][j / 2].hits <= 0)
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

void moveBallDownLeft(int indexOfTheBall)
{
	if (BallsAndDirections[indexOfTheBall][LeftDown] == 1 && display_draft[BallPosition[indexOfTheBall].y + 1][BallPosition[indexOfTheBall].x - 1] == 0 && BallPosition[indexOfTheBall].y < 24)
	{
		RemoveBall(indexOfTheBall);
		BallPosition[indexOfTheBall].y++;
		BallPosition[indexOfTheBall].x -= 2;
		DrawBall(indexOfTheBall);
		NoSound();
	}
	else
	{
		BallsAndDirections[indexOfTheBall][LeftDown] = 0;
		if (BallPosition[indexOfTheBall].x == 2) // western wall
			BallsAndDirections[indexOfTheBall][RightDown] = 1;
		else if (BallPosition[indexOfTheBall].y > 23) // fell down
			DeleteLife(indexOfTheBall);
		else if (BallPosition[indexOfTheBall].x >= RacketPosition && BallPosition[indexOfTheBall].x <= RacketPosition + sizeOfRacket && BallPosition[indexOfTheBall].y + 1 == 24 && greenSurFlag == 1)
		{
			display_draft[22][134] = '1';
			display_draft[22][134 + 1] = Yellow;
			clearAllDirections(indexOfTheBall);
		}
		else
		{
			BreakTheBrick(BallPosition[indexOfTheBall].y + 1, BallPosition[indexOfTheBall].x - 2, 0);
			BallsAndDirections[indexOfTheBall][LeftUp] = 1;
		}
	}
}

void moveBallDownRight(int indexOfTheBall)
{
	if (BallsAndDirections[indexOfTheBall][RightDown] == 1 && display_draft[BallPosition[indexOfTheBall].y + 1][BallPosition[indexOfTheBall].x + 3] == 0 && BallPosition[indexOfTheBall].y < 24)
	{
		RemoveBall(indexOfTheBall);
		BallPosition[indexOfTheBall].y++;
		BallPosition[indexOfTheBall].x += 2;
		DrawBall(indexOfTheBall);
		NoSound();
	}
	else
	{
		BallsAndDirections[indexOfTheBall][RightDown] = 0;
		if (BallPosition[indexOfTheBall].x == 98) // eastern wall
			BallsAndDirections[indexOfTheBall][LeftDown] = 1;
		else if (BallPosition[indexOfTheBall].y > 23) // fell down
			DeleteLife(indexOfTheBall);
		else if (BallPosition[indexOfTheBall].x >= RacketPosition && BallPosition[indexOfTheBall].x <= RacketPosition + sizeOfRacket && BallPosition[indexOfTheBall].y + 1 == 24 && greenSurFlag == 1)
		{
			display_draft[22][128] = '1';
			display_draft[22][128 + 1] = Yellow;
			clearAllDirections(indexOfTheBall);
		}
		else
		{
			BreakTheBrick(BallPosition[indexOfTheBall].y + 1, BallPosition[indexOfTheBall].x + 2, 0);
			BallsAndDirections[indexOfTheBall][RightUp] = 1;
		}
	}
}

void moveBallUpRight(int indexOfTheBall)
{
	if (BallsAndDirections[indexOfTheBall][RightUp] == 1 && display_draft[BallPosition[indexOfTheBall].y - 1][BallPosition[indexOfTheBall].x + 3] == 0)
	{
		RemoveBall(indexOfTheBall);
		BallPosition[indexOfTheBall].y--;
		BallPosition[indexOfTheBall].x += 2;
		DrawBall(indexOfTheBall);
	}
	else
	{
		BallsAndDirections[indexOfTheBall][RightUp] = 0;
		if (BallPosition[indexOfTheBall].x == 98) // eastern wall
			BallsAndDirections[indexOfTheBall][LeftUp] = 1;
		else
		{
			BreakTheBrick(BallPosition[indexOfTheBall].y - 1, BallPosition[indexOfTheBall].x + 2, 0);
			BallsAndDirections[indexOfTheBall][RightDown] = 1;
		}
	}
}

void moveBallUpLeft(int indexOfTheBall)
{
	if (BallsAndDirections[indexOfTheBall][LeftUp] == 1 && display_draft[BallPosition[indexOfTheBall].y - 1][BallPosition[indexOfTheBall].x - 1] == 0)
	{
		RemoveBall(indexOfTheBall);
		BallPosition[indexOfTheBall].y--;
		BallPosition[indexOfTheBall].x -= 2;
		DrawBall(indexOfTheBall);
	}
	else
	{
		BallsAndDirections[indexOfTheBall][LeftUp] = 0;
		if (BallPosition[indexOfTheBall].x == 2) // western wall
			BallsAndDirections[indexOfTheBall][RightUp] = 1;
		else
		{
			BreakTheBrick(BallPosition[indexOfTheBall].y - 1, BallPosition[indexOfTheBall].x - 2, 0);
			BallsAndDirections[indexOfTheBall][LeftDown] = 1;
		}
	}
}

void ballUpdater(int indexOfTheBall)
{
	while (1)
	{
		receive();
		if (BallsAndDirections[indexOfTheBall][RightUp])
			moveBallUpRight(indexOfTheBall);
		else if (BallsAndDirections[indexOfTheBall][LeftUp])
			moveBallUpLeft(indexOfTheBall);
		else if (BallsAndDirections[indexOfTheBall][RightDown])
			moveBallDownRight(indexOfTheBall);
		else if (BallsAndDirections[indexOfTheBall][LeftDown])
			moveBallDownLeft(indexOfTheBall);
	}
}

void orangeSurprise()
{
	ballSpeed = ballSpeed * 2;
	display_draft[20][120] = (ballSpeed / 10) + '0';
	display_draft[20][120 + 1] = Yellow;
	display_draft[20][120 + 2] = (ballSpeed % 10) + '0';
	display_draft[20][120 + 3] = Yellow;
	sleep(30);
	ballSpeed = ballSpeed / 2;
	display_draft[20][120] = (ballSpeed / 10) + '0';
	display_draft[20][120 + 1] = Yellow;
	display_draft[20][120 + 2] = (ballSpeed % 10) + '0';
	display_draft[20][120 + 3] = Yellow;
}

void BlueSurprise()
{
	sizeOfRacket = sizeOfRacket * 2;
	right = right + 10;
	DrawRacket();
	sleep(60);
	removeDoubleRacket();
	sizeOfRacket = sizeOfRacket / 2;
	right = 8;
	DrawRacket();
}

void greenSurprise()
{
	greenSurFlag = 1;
	sleep(60);
	greenSurFlag = 0;
}

void redSurprise()
{
	redSurFlag = 1;
	sleep(60);
	redSurFlag = 0;
}

/* triple the ball */
void whiteSurprise()
{
	int i, indexOfActiveBall;
	ballsCounter = amountOfBalls;
	for (i = 0; i < amountOfBalls; i++) // find the active ball
	{
		if (ballIsActive[i])
		{
			indexOfActiveBall = i;
			break;
		}
	}
	for (i = 0; i < amountOfBalls; i++) // update the coordinates of the active ball for the other balls   
	{
		if (i != indexOfActiveBall)
		{
			BallPosition[i].x = BallPosition[indexOfActiveBall].x;
			BallPosition[i].y = BallPosition[indexOfActiveBall].y;
		}
	}
	if (!BallOnRacket)
	{
		if (BallsAndDirections[indexOfActiveBall][RightUp])
		{
			BallsAndDirections[(indexOfActiveBall + 1) % amountOfBalls][LeftUp] = 1;
			BallsAndDirections[(indexOfActiveBall + 2) % amountOfBalls][LeftDown] = 1;
		}
		else if (BallsAndDirections[indexOfActiveBall][LeftUp])
		{
			BallsAndDirections[(indexOfActiveBall + 1) % amountOfBalls][RightUp] = 1;
			BallsAndDirections[(indexOfActiveBall + 2) % amountOfBalls][RightDown] = 1;
		}
		else if (BallsAndDirections[indexOfActiveBall][RightDown] || BallsAndDirections[indexOfActiveBall][LeftDown])
		{
			BallsAndDirections[(indexOfActiveBall + 1) % amountOfBalls][RightUp] = 1;
			BallsAndDirections[(indexOfActiveBall + 2) % amountOfBalls][LeftUp] = 1;
		}
	}
	for (i = 0; i < amountOfBalls; i++) // activate the suspended balls
	{
		if (!ballIsActive[i])
		{
			ballIsActive[i] = true;
			resume(ballPID[i]);
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
				{
					display_draft[18][120] = (surprisePosition[i].y / 10) + '0';
					display_draft[18][120 + 1] = Yellow;
					display_draft[18][120 + 2] = (surprisePosition[i].y % 10) + '0';
					display_draft[18][120 + 3] = Yellow;
					display_draft[19][120] = (surprisePosition[i].x / 10) + '0';
					display_draft[19][120 + 1] = Yellow;
					display_draft[19][120 + 2] = (surprisePosition[i].x % 10) + '0';
					display_draft[19][120 + 3] = Yellow;
					display_draft[18][130] = (i / 10) + '0';
					display_draft[18][130 + 1] = Yellow;
					display_draft[18][130 + 2] = (i % 10) + '0';
					display_draft[18][130 + 3] = Yellow;
					display_draft[20][130 + 2] = display_draft[surprisePosition[i].y][surprisePosition[i].x];
					display_draft[20][130 + 3] = display_draft[surprisePosition[i].y][surprisePosition[i].x + 1];
				}
				if (surprisePosition[i].y < 24)
				{
					drawSurprise(i, surpriseColor[i]);
				}
				else if (surprisePosition[i].x >= RacketPosition && surprisePosition[i].x <= RacketPosition + sizeOfRacket)
				{
					switch (surpriseColor[i])
					{
					case Blue:
						resume(create(BlueSurprise, INITSTK, INITPRIO, "BlueSurprise", 0));
						break;
					case Orange:
						resume(create(orangeSurprise, INITSTK, INITPRIO, "orangeSurprise", 0));
						break;
					case Green:
						resume(create(greenSurprise, INITSTK, INITPRIO, "greenSurprise", 0));
					case Red:
						resume(create(redSurprise, INITSTK, INITPRIO, "redSurprise", 0));
						break;
					case Purple:
						nextLevel(2);
						break;
					case White:
						whiteSurprise();
						break;
					}
				}
			}
		}
	}
}

void drawLazer(Position lazerPosition)
{
	display_draft[lazerPosition.y][lazerPosition.x] = 24; // arrow 
	display_draft[lazerPosition.y][lazerPosition.x + 1] = Yellow;
}

void removeLazer(Position lazerPosition)
{
	display_draft[lazerPosition.y][lazerPosition.x] = ' ';
	display_draft[lazerPosition.y][lazerPosition.x + 1] = 0;
}

void lazer()
{
	Position lazerPosition;
	lazerPosition.y = 23;
	if (sizeOfRacket == 10)
	{
		lazerPosition.x = RacketPosition + (sizeOfRacket / 2) - 1;
	}
	else
	{
		lazerPosition.x = RacketPosition + (sizeOfRacket / 2);
	}
	while (display_draft[lazerPosition.y - 1][lazerPosition.x + 1] == 0 && lazerPosition.y - 1 > 0)
	{
		lazerPosition.y--;
		drawLazer(lazerPosition);
		sleep(1);
		removeLazer(lazerPosition);
	}
	if (lazerPosition.y > 0)
	{
		BreakTheBrick(lazerPosition.y - 1, lazerPosition.x, 1);
		removeLazer(lazerPosition);
	}
}

INTPROC new_int9()
{
	char result = 0;
	int scan = 0, ascii = 0, i;
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
	if (scan == 3)
	{
		clrscr();
		send(butlerpid, MSGPSNAP);
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
		for (i = 128, j = 0; j < strlen(str); j++, i += 2)
		{
			display_draft[11][i] = str[j];
			display_draft[11][i + 1] = White;
		}
		sprintf(str, "%d", mytod);
		for (i = 126, j = 0; j < strlen(str); j++, i += 2)
		{
			display_draft[12][i] = str[j];
			display_draft[12][i + 1] = White;
		}
		b800h[1090] = (BallPosition[0].x / 10) + '0';
		b800h[1090 + 1] = Green;
		b800h[1090 + 2] = BallPosition[0].x % 10 + '0';
		b800h[1090 + 3] = Green;
		b800h[1250] = (BallPosition[0].y / 10) + '0';
		b800h[1250 + 1] = Green;
		b800h[1250 + 2] = BallPosition[0].y % 10 + '0';
		b800h[1250 + 3] = Green;
		display_draft[19][130] = RacketPosition / 10 + '0';
		display_draft[19][130 + 1] = Yellow;
		display_draft[19][130 + 2] = RacketPosition % 10 + '0';
		display_draft[19][130 + 3] = Yellow;
	} // while
} // displayer

void updater()
{
	char ch;
	int i, j, indexOfActiveBall;
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
				RemoveRacket(right);
				if (BallOnRacket)
				{
					for (i = 0; i < amountOfBalls; i++)
						if (ballIsActive[i])
							BallPosition[i].x -= 2;
				}
				RacketPosition -= 2;
				DrawRacket();
			}
			else if (((ch == 'r') || (ch == 'R')) && RacketPosition < 100 - sizeOfRacket)
			{
				RemoveRacket(left);
				if (BallOnRacket)
				{
					for (i = 0; i < amountOfBalls; i++)
						if (ballIsActive[i])
							BallPosition[i].x += 2;
				}
				RacketPosition += 2;
				DrawRacket();
			}
			else if (ch == 'f')
			{
				if (level == 1)
				{
					nextLevel(level);
				}
				else if (level == 2)
				{
					nextLevel(level);
				}
			}
			else if (ch == ' ')
			{
				if (BallOnRacket)
				{
					BallOnRacket = 0;
					for (i = 0; i < amountOfBalls; i++)
					{
						if (ballIsActive[i])
						{
							indexOfActiveBall = i;
							break;
						}
					}
					if (ballsCounter > 1)
					{
						BallsAndDirections[(indexOfActiveBall + 1) % amountOfBalls][LeftUp] = 1;
						BallsAndDirections[(indexOfActiveBall + 2) % amountOfBalls][RightUp] = 1;
						BallPosition[indexOfActiveBall].x -= 6;
						sleep(1); // to make the third ball fly in a little different angle
					}
					BallsAndDirections[indexOfActiveBall][RightUp] = 1;
				}
				if (redSurFlag)
					resume(create(lazer, INITSTK, INITPRIO, "Lazer", 0));
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
	for (i = 126, j = 0; i < 138; j++, i += 2) // draw score
	{
		display_draft[8][i] = scoreLabel[j];
		display_draft[8][i + 1] = White;
	}
	for (i = 126, j = 0; i <= 138; j++, i += 2) // draw life
	{
		display_draft[4][i] = lives[j];
		display_draft[4][i + 1] = White;

	}
	for (i = 128; i < 134; i += 2) // draw the hearts
	{
		display_draft[5][i] = 3;
		display_draft[5][i + 1] = 4;
	}
	send(lvlDrawerPID, 1); //send msg to lvl1drawder
}

void updateBrickMatrix(int i, int j, bool state, int hits, int score)
{
	matrix[i][j].enable = state;
	matrix[i][j].hits = hits * 2;
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

/* reset the ball and the racket to be in the middle */
void resetBallAndRacketPositions()
{
	int i, j;
	for (i = 2; i < 98; i += 2) // delete the whole racket
	{
		display_draft[24][i] = ' ';
		display_draft[24][i + 1] = 0;
	}
	for (i = 0; i < amountOfBalls; i++) // reset all the balls
	{
		if (ballIsActive[i])
		{
			RemoveBall(i);
			ballIsActive[i] = false;
			suspend(ballPID[i]);
		}
	}
	ballsCounter = 1;
	for (i = 0; i < 3; i++)
		for (j = 0; j < 4; j++)
			BallsAndDirections[i][j] = 0; // reset all the ball's directions 
	ballIsActive[0] = true;
	BallPosition[0].x = 50; // 3730
	BallPosition[0].y = 23;
	BallOnRacket = 1;
	RacketPosition = 46; // 3886
	DrawRacket();
	resume(ballPID[0]);
}

void lvl3Drawer()  //draw the second level
{
	int i, j, msg = receive(), randNum1, randNum2;
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
	resetBallAndRacketPositions();
	while (1)
	{
		randNum1 = (rand() % 96) + 2;
		if (randNum1 % 2 == 0)
		{
			randNum2 = (rand() % 2) + 1;
			display_draft[randNum2][randNum1] = 234;
			display_draft[randNum2][randNum1 + 1] = 3;
			sleep(1);
			display_draft[randNum2][randNum1] = ' ';
			display_draft[randNum2][randNum1 + 1] = 0;
		}
	}
}

void lvl2Drawer()  //draw the second level
{
	int i, j, msg = receive();
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
	resetBallAndRacketPositions();
}

void lvlDrawer()  //draw the first level
{
	int i, j, msg = receive();
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
							updateSurprises(i, j / 2, White);
							break;
						case 15:
							updateSurprises(i, j / 2, White);
							break;
						case 18:
							updateSurprises(i, j / 2, White);
							break;
						case 19:
							updateSurprises(i, j / 2, White);
							break;
						case 20:
							updateSurprises(i, j / 2, White);
							break;
						case 21:
							updateSurprises(i, j / 2, White);
							break;
						case 22:
							updateSurprises(i, j / 2, White);
							break;
						case 23:
							updateSurprises(i, j / 2, White);
							break;
						case 24:
							updateSurprises(i, j / 2, White);
							break;
						}
					}
				}
			}
		}
	}
	resetBallAndRacketPositions();
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
	int i, j;
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
	b800h = (unsigned char far*)0xB8000000;
	PositionOfTheLastLife = 132;
	lifeCounter = 3;
	sizeOfRacket = 10;
	right = 8;
	left = 0;
	surprisesIndex = 0;
	redSurFlag = greenSurFlag = 0;
	hertz = 500;
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
	int i, j, uppid, dispid, recvpid, frameDrawPID, lvlDrawerPID, dropSurPID;
	InitializeGlobalVariables();
	initBrick();
	setInt70h();
	for (i = 0; i < amountOfBalls; i++) // creating the balls
		ballPID[i] = create(ballUpdater, INITSTK, INITPRIO, "BallUpdater", 1, i);
	resume(lvlDrawerPID = create(lvlDrawer, INITSTK, INITPRIO, "lvlDrawer", 0));
	resume(lvl2DrawerPID = create(lvl2Drawer, INITSTK, INITPRIO, "lvl2Drawer", 0));
	resume(lvl3DrawerPID = create(lvl3Drawer, INITSTK, INITPRIO, "lvl3Drawer", 0));
	resume(frameDrawPID = create(frameDraw, INITSTK, INITPRIO + 4, "FrameDraw", 1, lvlDrawerPID));
	resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
	resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
	resume(uppid = create(updater, INITSTK, INITPRIO, "UPDATER", 0));
	resume(dropSurPID = create(dropSur, INITSTK, INITPRIO, "dropSur", 0));
	receiver_pid = recvpid;
	set_new_int9_newisr();
	schedule(7, 2, dispid, 0, uppid, 1, frameDrawPID, 1, ballPID[0], 1, dropSurPID, 1, ballPID[1], 1, ballPID[2], 1);
} // xmain