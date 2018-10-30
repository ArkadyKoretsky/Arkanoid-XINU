while(1)
		{
			randNum1 = (rand() % 96) +2;
			if (randNum1 % 2 == 0)
			{
				randNum2= (rand() % 2)+1;
				display_draft[randNum2][randNum1] = 219;
				display_draft[randNum2][randNum1+1] = 3;
				sleep(1);
				display_draft[randNum2][randNum1] = ' ';
				display_draft[randNum2][randNum1+1] = 0;
			}
				
		
		}