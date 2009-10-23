#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

int master_pty;
int slave_pty;

char *term = NULL;
struct termios term_settings;

extern char *ptsname();

int pty_fork();
int get_pty(int *master, int *slave);
void sigchld_handler(int sig_num);

int main(int argc, char *argv[])
{
	struct sigaction chld;
	memset(&chld, 0, sizeof(chld));
	chld.sa_handler = &sigchld_handler;

	int pid;
	term = getenv("TERM");

	if(argc < 2)
		fprintf(stderr, "Improper number of arguments.\n");


	if((pid = pty_fork()) == 0) {  //if child
		setenv("TERM", term, 1);
		execvp(argv[1], argv+1);
	} else {
	sigaction(SIGCHLD, &chld, NULL);
		char in[1024];
		int ret;
		int standard_in;
		dup2(standard_in, STDIN_FILENO);

		while(1) {
			int nfds=0;

			fd_set rd, wr, er;
			FD_ZERO(&rd);
			FD_ZERO(&wr);
			FD_ZERO(&er);

			nfds = max(nfds, standard_in);
			FD_SET(standard_in, &rd);

			nfds = max(nfds, master_pty);
			FD_SET(master_pty, &rd);

			int r = select(nfds+1, &rd, &wr, &er, NULL);

			if(r == -1)
				continue;

			if(FD_ISSET(standard_in, &rd)) {
				ret = read(standard_in, in, 1024*sizeof(char));
				write(master_pty, in, ret*sizeof(char));
			}
			if(FD_ISSET(master_pty, &rd)) {
				ret = read(master_pty, in, 1024*sizeof(char));
				write(STDOUT_FILENO, in, ret*sizeof(char));
			}
		}
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
		int tty = open("/dev/tty", O_RDWR | O_NOCTTY);

		tcgetattr(tty, &term_settings);
		cfmakeraw(&term_settings);
		tcsetattr(tty, TCSANOW, &term_settings);
		close(tty);
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

	*slave = open(name, O_RDWR | O_NOCTTY, 0);

	return 0;
}

void sigchld_handler(int sig_num)
{
	exit(EXIT_SUCCESS);
}
