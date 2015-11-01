#include <cstdio>
#include "examples.h"

int main(int argc, char **argv)
{
	BasicUsageExample();
	printf("Press enter to continue\n"); getchar();
	BasicSharedAuthorityExample();
	printf("Press enter to continue\n"); getchar();
	MultithreadedWorldProcessingExample();
	printf("Press enter to continue\n"); getchar();
	return 0;
}