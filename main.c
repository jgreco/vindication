#define _GNU_SOURCE
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

/* for portable behaviour, curses input/output is never used */
#include <curses.h>
#include <term.h>


#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUFFER 1024

/*
#define DEBUG
*/


#ifdef DEBUG
FILE *log_f;
#endif

int master_controlling_tty;
int master_pty;
int slave_pty;

int pid;

char *term = NULL;
struct termios term_settings;
struct termios orig_settings;
struct winsize term_size;

enum mode { INSERT, NORMAL};
int state;

char *K_END;    int S_END;
char *K_HOME;   int S_HOME;
char *K_ENTER;  int S_ENTER;
char *K_UP;     int S_UP;
char *K_DOWN;   int S_DOWN;
char *K_LEFT;   int S_LEFT;
char *K_RIGHT;  int S_RIGHT;
char *K_DELETE; int S_DELETE;

char mangled_in[BUFFER];
int mangled_len;
int command_count;

int pty_fork();
int get_pty(int *master, int *slave);
void sigchld_handler(int sig_num);
void sigwinch_handler(int sig_num);

void setup_escape_seqs();
void input_mangle(char *in, int num);

int main(int argc, char *argv[])
{
	struct sigaction chld;
	struct sigaction winch;
	int STUPID;

#ifdef DEBUG
	log_f = fopen("dump", "w");
#endif

	memset(&chld, 0, sizeof(chld));
	memset(&winch, 0, sizeof(winch));
	chld.sa_handler = &sigchld_handler;
	winch.sa_handler = &sigwinch_handler;

	term = getenv("TERM");
	state = INSERT;
	mangled_len = 0;
	command_count = 0;

	STUPID = open("/dev/null", O_RDWR);  /* setupterm needs a file descriptor but we don't want it to mess around with our real terminal */

	if(setupterm((char *)0, STUPID, (int *)0) == ERR) {

		fprintf(stderr, "could not get terminfo.\n");
		exit(EXIT_FAILURE);
	}
	close(STUPID);

	setup_escape_seqs();

	master_controlling_tty = open("/dev/tty", O_RDWR | O_NOCTTY);
	ioctl(master_controlling_tty, TIOCGWINSZ, &term_size);  /* save terminal size */

	if(argc < 2) {
		fprintf(stderr, "Improper number of arguments.\n");
		exit(EXIT_FAILURE);
	}


	if((pid = pty_fork()) == 0) {  /* if child */
		setenv("TERM", term, 1);
		execvp(argv[1], argv+1);
	} else {
		char in[BUFFER];
		int ret;
		int standard_in = dup(STDIN_FILENO);

		tcgetattr(master_controlling_tty, &term_settings);
		tcgetattr(master_controlling_tty, &orig_settings);
		cfmakeraw(&term_settings);
		term_settings.c_cc[VMIN] = 1;
		term_settings.c_cc[VTIME] = 1;

		tcsetattr(master_controlling_tty, TCSANOW, &term_settings);

		sigaction(SIGCHLD, &chld, NULL);
		sigaction(SIGWINCH, &winch, NULL);

		while(1) {
			int nfds=0;
			int r;

			fd_set rd, wr, er;
			FD_ZERO(&rd);
			FD_ZERO(&wr);
			FD_ZERO(&er);

			nfds = max(nfds, standard_in);
			FD_SET(standard_in, &rd);

			nfds = max(nfds, master_pty);
			FD_SET(master_pty, &rd);

			r = select(nfds+1, &rd, &wr, &er, NULL);

			if(r == -1)
				continue;

			if(FD_ISSET(standard_in, &rd)) {
				ret = read(standard_in, in, BUFFER*sizeof(char));

				input_mangle(in, ret);

				write(master_pty, mangled_in, mangled_len*sizeof(char));
			}
			if(FD_ISSET(master_pty, &rd)) {
				ret = read(master_pty, in, BUFFER*sizeof(char));
				write(STDOUT_FILENO, in, ret*sizeof(char));
			}
		}
	}

	return 0;
}


int pty_fork()
{
	int n_pid;
	int master, slave;
	int fd;

	get_pty(&master, &slave);

	if((n_pid= fork()) < 0) {
		fprintf(stderr, "FORK FAILED\n");
		exit(EXIT_FAILURE);
	} else if(n_pid == 0) {  /* child */
		close(master);

		/* make tty the controlling tty */
		setsid();

		fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		ioctl(fd, TIOCNOTTY, NULL);  /* kill our controlling tty */
		close(fd);

		fd = ioctl(slave, TIOCSCTTY, NULL);

		ioctl(slave, TIOCSWINSZ, &term_size);  /* set terminal size */

		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);
	} else {  /* parent */
		/* parent junk */
	}

	master_pty = master;
	slave_pty = slave;


	return n_pid;
}

int get_pty(int *master, int *slave)
{
	int num;
	char *name;

	*master = getpt();
	grantpt(*master);
	unlockpt(*master);

	num = *master;
	name = ptsname(num);

	*slave = open(name, O_RDWR | O_NOCTTY, 0);

	return 0;
}

void sigchld_handler(int sig_num)
{
	tcsetattr(master_controlling_tty, TCSANOW, &orig_settings);
	exit(EXIT_SUCCESS);
}

void sigwinch_handler(int sig_num)
{
	ioctl(master_controlling_tty, TIOCGWINSZ, &term_size);  /* save new terminal size */

	ioctl(slave_pty, TIOCSWINSZ, &term_size);  /* set terminal size */

	kill(pid, SIGWINCH);
}

void input_mangle(char *in, int num)
{
	int i, k;

	mangled_len = 0;

	if(num == 1 && in[0] == 27) {
		if(state == INSERT) {
			mangled_in[mangled_len++] = 27;
			mangled_in[mangled_len++] = 91;
			mangled_in[mangled_len++] = 68;

			mangled_len = 3;

			state = NORMAL;
			return;
		}

		else if(state == NORMAL) {
			state = INSERT;
			mangled_len = 1;
			mangled_in[0] = 27;
			return;
		}
	}


	for(i=0; i<num; i++) {
		if(state == INSERT) {
			mangled_in[mangled_len++] = in[i];
		}

		else if(state == NORMAL) {

			if(isdigit(in[i])) {
				if(command_count == 1)
					command_count = in[i] - '0';
				else
					command_count = command_count * 10 + in[i] - '0';

				continue;
			}

			for(k=0; k<command_count; k++) {  /* FIXME turn into do-while loop, allow for 10-19 as count values */
			switch(in[i]) {
				case 13:
					mangled_in[mangled_len++] = 13;
					break;
				case 'i':
					state = INSERT;
					break;
				case 'A':
					state = INSERT;
					strcpy(mangled_in+mangled_len, K_END);
					mangled_len += S_END;
					break;
				case 'h':
					strcpy(mangled_in+mangled_len, K_LEFT);
					mangled_len += S_LEFT;
					break;
				case 'j':
					strcpy(mangled_in+mangled_len, K_DOWN);
					mangled_len += S_DOWN;
					break;
				case 'k':
					strcpy(mangled_in+mangled_len, K_UP);
					mangled_len += S_UP;
					break;

				case 'a':
					state = INSERT;
				case 'l':
					strcpy(mangled_in+mangled_len, K_RIGHT);
					mangled_len += S_RIGHT;
					break;

				case 'x':
					strcpy(mangled_in+mangled_len, K_DELETE);
					mangled_len += S_DELETE;
					break;
			}
			}

			command_count = 1;
		}
	}

	return;
}

void setup_escape_seqs()
{
	K_DELETE = tigetstr("kdch1"); S_DELETE = strlen(K_DELETE);

	K_HOME   = tigetstr("khome"); S_HOME   = strlen(K_HOME);
	K_END    = tigetstr("kend");  S_END    = strlen(K_END);
	K_ENTER  = tigetstr("kent");  
	if(K_ENTER <= (char *)0) {
#ifdef DEBUG
	fprintf(log_f, "enter not in terminfo"); fflush(log_f);
#endif
		K_ENTER = strdup("\r");
		S_ENTER = 1;
	}
	else
		S_ENTER  = strlen(K_ENTER);

	K_UP     = tigetstr("kcuu1"); S_UP     = strlen(K_UP);
	K_DOWN   = tigetstr("kcud1"); S_DOWN   = strlen(K_DOWN);
	K_LEFT   = tigetstr("kcub1"); S_LEFT   = strlen(K_LEFT);
	K_RIGHT  = tigetstr("kcuf1"); S_RIGHT  = strlen(K_RIGHT);

	return;
}
