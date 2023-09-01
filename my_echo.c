
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]){
   int answer = 0;

   if (argc>1)
	    answer = atoi(argv[1]);

   printf("my answer is %d\n", answer);  
   return answer;
}
