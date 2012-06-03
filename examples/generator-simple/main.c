#include <stdio.h>

extern char generated_data[];

int main(int argc, char* argv[]) {
	printf("Generated data: %s\n", generated_data);
}
