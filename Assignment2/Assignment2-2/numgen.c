#include <stdio.h>
#include <stdlib.h>

#ifndef MAX_PROCESSES
#define MAX_PROCESSES 8
#endif

int main()
{
	const int N = MAX_PROCESSES * 2;
	FILE* f = fopen("./temp.txt","w"); // open  temp.txt

	// except 
	if(!f){
		perror("fopen");
		return 1;
	}

	// 생성할 프로세스 수의 2배만큼 i번째 값을 기록
	for(int i = 1; i<=N;i++)
		fprintf(f,"%d\n",i);
	
	fclose(f); // close temp.txt

	return 0;

}
