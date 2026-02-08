#include <stdio.h>
#include <stdlib.h>

/*
* MAX_PROCESSES
* 
* Defines the maximum number of processes to be handled
* by the experiment. if not provided at compile time,
* it defaults to 8
*/

#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

int main()
{
	const int N = MAX_PROCESSES * 2;
	FILE* f = fopen("./temp.txt","w"); 

	if(!f){
		perror("fopen");
		return 1;
	}

	for(int i = 1; i<=N;i++)
		fprintf(f,"%d\n",i);
	
	fclose(f); 

	return 0;

}
