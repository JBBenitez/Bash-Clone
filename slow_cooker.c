
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define N 10

int main(int argc, char *argv[]){
   int counter;

   if (argc==1)
	counter = N;
   else
	counter = atoi(argv[1]);
	
   while (counter>=0){
  	printf("slow_cooker count down: %d ...\n", counter);
 	fflush(stdout);
	counter--;
	sleep(1);
   }

   return 0;
}
