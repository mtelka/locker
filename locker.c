/* Public Domain */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

static int
convert_int(int *val, char *str)
{
	long lval;

	if (str == NULL || !isdigit(*str))
		return (-1);

	lval = strtol(str, &str, 10);
	if (*str != '\0' || lval > INT_MAX)
		return (-2);

	*val = (int)lval;
	return (0);
}

void
lock_cmd_usage(void) {
	printf("Usage: lock r|R|w|W|u|U start len filename\n");
}

void
test_cmd_usage(void) {
	printf("Usage: test r|w|u start len filename\n");
}

int
locktest_cmd(int argc, char *argv[])
{
	int f;
	struct flock fl;
	int cmd;
	int t;
	void (*usage_fnc)(void);

	if (argc < 1)
		return -1;

	if (strcmp(argv[0], "lock") == 0) {
		usage_fnc = lock_cmd_usage;
		cmd = F_SETLK;
	} else if (strcmp(argv[0], "test") == 0) {
		usage_fnc = test_cmd_usage;
		cmd = F_GETLK;
	} else {
		return -1;
	}

	if (argc != 5 || strlen(argv[1]) != 1) {
		usage_fnc();
		return -1;
	}

	f = open(argv[4], O_RDWR | O_CREAT, 0777);
	if (f == -1) {
		printf("open failed\n");
		return -1;
	}

	switch (argv[1][0]) {
	case 'r':
	case 'w':
	case 'u': break;
	case 'R':
	case 'W':
	case 'U':
		if (cmd == F_SETLK) {
			cmd = F_SETLKW;
			break;
		}
	default: usage_fnc(); return -1;
	}

	switch (argv[1][0]) {
	case 'r':
	case 'R': fl.l_type = F_RDLCK; break;
	case 'w':
	case 'W': fl.l_type = F_WRLCK; break;
	case 'u':
	case 'U': fl.l_type = F_UNLCK; break;
	default: usage_fnc(); return -1;
	}

	fl.l_whence = SEEK_SET;

	if (convert_int(&t, argv[2]) != 0) {
		usage_fnc(); return -1;
	}
	fl.l_start = t;
	if (convert_int(&t, argv[3]) != 0) {
		usage_fnc(); return -1;
	}
	fl.l_len = t;

	if (fcntl(f, cmd, &fl) == -1) {
		printf("fcntl failed: (%d) %s\n",
		    errno, strerror(errno));
		(void) close(f);
		return 0;
	}

	if (cmd == F_GETLK) {
		char *s;

		switch (fl.l_type) {
		case F_WRLCK: s = "F_WRLCK"; break;
		case F_RDLCK: s = "F_RDLCK"; break;
		case F_UNLCK: s = "F_UNLCK"; break;
		default: s = "unknown"; break;
		}

		printf("F_GETLK: %s %ld %ld\n", s, fl.l_start, fl.l_len);

		(void) close(f);
	}

	return 0;
}

int
sleep_cmd(int argc, char *argv[])
{
	int sec;

	if (argc != 2 || convert_int(&sec, argv[1]) != 0) {
		printf("Usage: sleep seconds\n");
		return -1;
	}

	(void) sleep(sec);

	return 0;
}

void
usage(void)
{
	printf("syntax error\n");
	printf("\nSupported commands:\n\tlock\n\ttest\n\tsleep\n");
}

void
parse_cmd_v(int argc, char *argv[])
{
	if (argc < 1) {
		usage();
		return;
	}

	if (strcmp(argv[0], "lock") == 0)
		(void) locktest_cmd(argc, argv);
	else if (strcmp(argv[0], "test") == 0)
		(void) locktest_cmd(argc, argv);
	else if (strcmp(argv[0], "sleep") == 0)
		(void) sleep_cmd(argc, argv);
	else
		usage();
}

void
parse_cmd(char *s)
{
	char **argv = NULL;
	int argc = 0;
	int argc_max = 0;

	for (;;) {
		while (*s != '\0' && !isgraph(*s))
			*s++ = '\0';

		if (*s == '\0')
			break;

		if (argc == argc_max) {
			char **new_argv;
			argc_max += 4;
			new_argv = realloc(argv, argc_max * sizeof *argv);
			if (new_argv == NULL) {
				printf("memory allocation failed\n");
				free(argv);
				return;
			}
			argv = new_argv;
		}

		argv[argc++] = s;

		while (*s != '\0' && isgraph(*s))
			s++;
	}

	if (argc == 0) {
		free(argv);
		return;
	}

	parse_cmd_v(argc, argv);

	free(argv);
}

#include <signal.h>

static void
siglost(int sig)
{
	printf("Signal %d received.\n", sig);
}

int
main(int argc, char *argv[])
{
	char *line;
	size_t len;

	if (argc == 1) {
		struct sigaction act;

		(void) memset(&act, 0, sizeof (act));

		act.sa_handler = &siglost;
		act.sa_flags = SA_RESTART;
#ifndef __linux__
		if (sigaction(SIGLOST, &act, NULL) < 0) {
			printf("sigaction failed\n");
		}
#endif
		if (sigaction(SIGINT, &act, NULL) < 0) {
			printf("sigaction failed\n");
		}

		for (;;) {
			line = NULL;
			len = 0;
			printf("> ");
			fflush(stdout);
			if (getline(&line, &len, stdin) == -1)
				break;

			parse_cmd(line);
			free(line);
		}
		printf("\n");
	} else {
		parse_cmd_v(argc - 1, argv + 1);
	}

	return 0;
}
