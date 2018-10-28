#include <stdio.h>

#include <stdlib.h>

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

unsigned char far* b800h;

enum color {
    Gray = 128, White = 240, Orange = 192,
    Blue = 16, Green = 32, Red = 64,
    Yellow = 224, Purple = 80
};


void drawRacket()
{
    int i = 0;
    for (i = 0; i < 6; i += 2)
    {
        b800h[1948*2 + i] = 220;
        b800h[1948*2 + i + 1] = 112;
    }
    b800h[1870*2] = 'o';
    b800h[1870*2 + 1] = 256;
    b800h[1870*2 + 1] = 4;
}

void lvl1print()
{
    int i;
    for (i = 0; i < 4000; i += 2)
    {
        if (i == 0)
        {
            b800h[i] = 201;
            b800h[i + 1] = 112;
        }
        else if (i < 100) // upper row
        {
            b800h[i] = 205;
            b800h[i + 1] = 112;
        }
        else if (i == 100)
        {
            b800h[i] = 187;
            b800h[i + 1] = 112;
        }
        else if (i % 160 == 0 || (i + 60) % 160 == 0)
        {
            b800h[i] = 186;
            b800h[i + 1] = 112;
        }
        else
        {
            b800h[i] = ' ';
            b800h[i + 1] = 0;
        }
    }
}

void main()
{
    int lvl1matrix[25][80] = { 0 };
    int i, j, space = 0;
    char* arkanoid = "Arkanoid";
    char* scoreLabel = "Score:";
    b800h = (unsigned char far*)0xB8000000;
    asm{
        PUSH AX
            XOR AH,AH
            MOV AL,3
            INT 10h
            POP AX
    }
    lvl1print();
    /*
for (i = 17; i < 22; i++)  //only the upper half of the screen
{
	for (j = 1; j < 39; j++)
	{
		switch (i)
		{
		case 17:
			lvl1matrix[i][j] = Gray;
			break;
		case 18:
			lvl1matrix[i][j] = Red;
			break;
		case 19:
			lvl1matrix[i][j] = Yellow;
			break;
		case 20:
			lvl1matrix[i][j] = Blue;
			break;
		case 21:
			lvl1matrix[i][j] = Green;
			break;
		}
	}
}
*/
    for (i = 0; i < 4000; i += 2)
    {
        //b800h[i] = '
        if (space == 0)
        {
            if (i >= 482 && i <= 578)
            {
                b800h[i + 1] = White;
            }
            else if (i >= 642 && i <= 738)
            {
                b800h[i + 1] = Red;

            }
            else if (i >= 802 && i <= 898)
            {
                b800h[i + 1] = Yellow;

            }
            else if (i >= 962 && i <= 1058)
            {
                b800h[i + 1] = Blue;

            }
            else if (i >= 1122 && i <= 1218)
            {
                b800h[i + 1] = Green;

            }
            space = 1;
        }
        else
        {
            space = 0;
        }
    }
    drawRacket();

    for (i = 142, j = 0; i < 158; j++, i += 2)				// draw arkanoid
    {
        b800h[i] = arkanoid[j];
        b800h[i + 1] = White;
    }
    for (i = 224, j = 0; i < 236; j++, i += 2) // draw score
    {
        b800h[i] = scoreLabel[j];
        b800h[i + 1] = White;
    }
    for(i=120;i<;i++)
    {
b800h[120]
    }
    for (i = 132; i < 138; i += 2) // draw life
    {
        b800h[i] = 3;
        b800h[i + 1] = 4;
    }
    sleep(10);
    asm{
        PUSH AX
            MOV AX, 2
            INT 10h
            POP AX
    }
}