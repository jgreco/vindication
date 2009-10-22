#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>

int master_pty;
int slave_pty;

char *term = NULL;

extern char *ptsname();

int pty_fork();
int get_pty(int *master, int *slave);

int main(int argc, char *argv[])
{
	int pid;
	term = getenv("TERM");

	if(argc < 2)
		fprintf(stderr, "Improper number of arguments.\n");


	if((pid = pty_fork()) == 0) {  //if child
		setenv("TERM", term, 1);
		execvp(argv[1], argv+1);
	} else {
		printf("parent pid: %d\n", pid);
	}

	return 0;
}


int pty_fork()
{
	int pid;
	int master, slave;

	get_pty(&master, &slave);

	if((pid= fork()) < 0) {
		fprintf(stderr, "FORK FAILED\n");
		exit(EXIT_FAILURE);
	} else if(pid == 0) {  //child
		close(master);  //child should learn it's place

		//make tty the controlling tty
		pid_t ret = setsid();

		int fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		ioctl(fd, TIOCNOTTY, NULL);  //kill our controlling tty
		close(fd);

		fd = ioctl(slave, TIOCSCTTY, NULL);

		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO); 

	} else {  //parent
		//parent junk

	}

	master_pty = master;
	slave_pty = slave;


	return pid;
}

int get_pty(int *master, int *slave)
{
	*master = getpt();
	grantpt(*master);
	unlockpt(*master);

	int num = *master;
	char *name = ptsname(num);
	printf("name: %s\n", name);

	*slave = open(name, O_RDWR | O_NOCTTY, 0);

	return 0;
}
