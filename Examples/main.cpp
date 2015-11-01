#include <cstdio>
#include "examples.h"

int main(int argc, char **argv)
{
	BasicUsageExample();
	getchar();
	BasicSharedAuthorityExample();
	getchar();
	MultithreadedWorldProcessingExample();
	getchar();
	return 0;
}