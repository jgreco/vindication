#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int master_pty;
int slave_pty;

//struct termios saved_term_settings;

int get_pty();

int main(int argc, char *argv[])
{
	int pid;

	if(argc < 2)
		fprintf(stderr, "Improper number of arguments.\n");


	if((pid = fork()) == 0) {  //if child
		execvp(argv[1], argv+2);
	} else {
		printf("parent pid: %d\n", pid);
	}

	return 0;
}

int get_pty()
{
	return getpt();
}
