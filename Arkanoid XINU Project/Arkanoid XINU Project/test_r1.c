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

extern SYSCALL  sleept(int);
extern struct intmap far *sys_imp;
int receiver_pid;
volatile int RacketPosition = 3886;
INTPROC old_Inc9;
unsigned char far* b800h;
char display[2001];
char ch_arr[2048];
int point_in_cycle;
int gcycle_length;
int gno_of_pids;

enum color {
    Gray = 128, White = 240, Orange = 192,
    Blue = 16, Green = 32, Red = 64,
    Yellow = 224, Purple = 80
};

void drawRacket()
{
    int i = 0;
    for (i = 0; i < 10; i += 2)
    {
        b800h[RacketPosition + i] = 220;
        b800h[RacketPosition + i + 1] = 112;
    }
    b800h[RacketPosition-156] = 'o';
    b800h[RacketPosition-156+ 1] = 4;
}

void RemoveRacket() /* removing the parts of the racket and the ball */
{
    int i;
    b800h[RacketPosition - 156] = ' '; // deleting the ball
    b800h[RacketPosition - 156 + 1] = 0;
    for (i = 0; i < SizeOfRacket; i += 2) // deleting the racket
    {
        b800h[RacketPosition + i] = ' ';
        b800h[RacketPosition + i + 1] = 0;
    }
}

INTPROC new_int9(int mdevno)
{
    char result = 0;
    int scan = 0;
    int ascii = 0;
    asm{
        PUSH AX
            MOV AH, 1
            INT 16h
            JZ EndInterrupt
            MOV AH, 0
            INT 16h
            MOV BYTE PTR scan, AH
            MOV BYTE PTR ascii, AL
    } //asm
    if (scan == 75 && RacketPosition > 3842) // left arrow and not at the left corner
    {
        RemoveRacket();
        RacketPosition -= 2;
        result = 'a';
        drawRacket();
    }
    else if (scan == 77 && RacketPosition < 3930) // right arrow and not at the right corner
    {
        RemoveRacket();
        RacketPosition += 2;
        result = 'd';
        drawRacket();
    }
    else if (scan == 57) // space was pressed
    {
        result = ' ';
        // release the ball\fire lazers
        goto EndInterrupt;
    }
    else if ((scan == 46) && (ascii == 3)) // Ctrl-C?
        asm{ INT 27 } // terminate xinu
    EndInterrupt:
    send(receiver_pid, result);
    asm{
        MOV AL, 20h
            OUT 20h, AL
            POP AX
    }
} // new_int9

void set_new_int9_newisr()
{
    int i;
    for (i = 0; i < 32; i++)
        if (sys_imp[i].ivec == 9)
        {
            old_Inc9=sys_imp[i].newisr;
            sys_imp[i].newisr = new_int9;
            return;
        }

} // set_new_int9_newisr

void set_old_int9_newisr()
{
    int i;
    for (i = 0; i < 32; i++)
        if (sys_imp[i].ivec == 9)
        {
            sys_imp[i].newisr = old_Inc9;
            return;
        }

} // set_new_int9_newisr

int front = -1;
int rear = -1;

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
        //getc(CONSOLE);
    } // while

} //  receiver

void displayer(void)
{
    while (1)
    {
        receive();
        //sleept(18);
        //printf(display);
    } //while
} // prntr

void updateter()
{

    char ch;
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

            if ((ch == 'l') || (ch == 'L'))
            {
                RemoveRacket();
                RacketPosition -= 2;
                drawRacket();
            }
            else if ((ch == 'r') || (ch == 'R'))
            {
                RemoveRacket();
                RacketPosition += 2;
                drawRacket();
            }
            /*else if ((ch == 'w') || (ch == 'W'))
				if (no_of_arrows < ARROW_NUMBER)
				{
					arrow_pos[no_of_arrows].x = gun_position;
					arrow_pos[no_of_arrows].y = 23;
					no_of_arrows++;*/

        } // if
    } // while(front != -1)

}

int sched_arr_pid[5] = { -1 };
int sched_arr_int[5] = { -1 };

SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...)
{
    int i;
    int ps;
    int *iptr;

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
    int i, j, space = 0;
    char* arkanoid = "Arkanoid";
    char* scoreLabel = "Score:";
    int uppid, dispid, recvpid;
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
    }
    resume(dispid = create(displayer, INITSTK, INITPRIO, "DISPLAYER", 0));
    resume(recvpid = create(receiver, INITSTK, INITPRIO + 3, "RECIVEVER", 0));
    resume(uppid = create(updateter, INITSTK, INITPRIO, "UPDATER", 0));
    receiver_pid = recvpid;
    set_new_int9_newisr();
    schedule(2, 10, dispid, 0, uppid, 5);
    sleep(10);
    set_old_int9_newisr();
    asm{
        PUSH AX
            MOV AX, 2
            INT 10h
            POP AX
    }
}