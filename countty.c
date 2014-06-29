#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


static int countdown(long int, int);
static void countup(long int *, long int *);
static void full_color(int *);
static void render_duration(long int, int);
static void render_glyph_row(char, int, char *);
static void restore_cursor(void);
static void restore_cursor_and_quit(int);
static void wait_for_next_second(long int);


#define FONT_HEIGHT 7
#define FONT_WIDTH 8
#define FONT_CHARACTERS 13

static unsigned char font[][FONT_CHARACTERS] = {
	/* Line by line, top to bottom, least significant bit is the
	 * right-most character: 0x3D = 0b00111101 = "  xxxx x". */
	{ '0', 0x3C, 0x42, 0x42, 0x00, 0x42, 0x42, 0x3C },
	{ '1', 0x00, 0x02, 0x02, 0x00, 0x02, 0x02, 0x00 },
	{ '2', 0x3C, 0x02, 0x02, 0x3C, 0x40, 0x40, 0x3C },
	{ '3', 0x3C, 0x02, 0x02, 0x3C, 0x02, 0x02, 0x3C },
	{ '4', 0x00, 0x42, 0x42, 0x3C, 0x02, 0x02, 0x00 },
	{ '5', 0x3C, 0x40, 0x40, 0x3C, 0x02, 0x02, 0x3C },
	{ '6', 0x3C, 0x40, 0x40, 0x3C, 0x42, 0x42, 0x3C },
	{ '7', 0x3C, 0x02, 0x02, 0x00, 0x02, 0x02, 0x00 },
	{ '8', 0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C },
	{ '9', 0x3C, 0x42, 0x42, 0x3C, 0x02, 0x02, 0x3C },
	{ 'd', 0x00, 0x02, 0x02, 0x3C, 0x42, 0x42, 0x3C },
	{ ':', 0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x00 },
	{ ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};


int
countdown(long int target, int critical)
{
	struct timeval tv;
	long int diff;

	if (gettimeofday(&tv, NULL) == -1)
	{
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	diff = target - tv.tv_sec;
	if (diff <= 0)
		return 1;

	render_duration(diff, critical);
	return 0;
}

void
countup(long int *ref, long int *sync_fraction)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1)
	{
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	if (*ref == 0)
		*ref = tv.tv_sec;

	if (*sync_fraction == 0)
		*sync_fraction = tv.tv_usec * 1e3;

	render_duration(tv.tv_sec - *ref, -1);
}

void
full_color(int *blink)
{
	int x, y;
	struct winsize w;

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	fputs("\033[H", stdout);
	fputs(*blink % 2 ? "\033[0m" : "\033[7;1;31m", stdout);
	for (y = 0; y < w.ws_row; y++)
		for (x = 0; x < w.ws_col; x++)
			putchar(' ');
	fflush(stdout);
	(*blink)--;
}

void
render_duration(long int s, int critical)
{
	long int sm = s;
	int i, y, days, hours, minutes, seconds;
	int pad_x, pad_y, rest_x, rest_y;
	char buf[32] = "", *p;
	struct winsize w;

	days = sm / 86400;
	sm %= 86400;
	hours = sm / 3600;
	sm %= 3600;
	minutes = sm / 60;
	seconds = sm % 60;

	if (days > 0)
		snprintf(buf, 32, "%dd %02d:%02d:%02d", days, hours, minutes, seconds);
	else if (hours > 0)
		snprintf(buf, 32, "%02d:%02d:%02d", hours, minutes, seconds);
	else if (minutes > 0)
		snprintf(buf, 32, "%02d:%02d", minutes, seconds);
	else
		snprintf(buf, 32, "%02d", seconds);

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	pad_x = ((int)w.ws_col - (int)strlen(buf) * FONT_WIDTH) / 2;
	rest_x = (int)w.ws_col - pad_x - (int)strlen(buf) * FONT_WIDTH;
	pad_y = ((int)w.ws_row - FONT_HEIGHT) / 2;
	rest_y = (int)w.ws_row - pad_y - FONT_HEIGHT;

	fputs("\033[H", stdout);
	for (y = 0; y < pad_y; y++)
		for (i = 0; i < w.ws_col; i++)
			putchar(' ');
	for (y = 0; y < FONT_HEIGHT; y++)
	{
		for (i = 0; i < pad_x; i++)
			putchar(' ');
		for (p = buf; *p; p++)
			if (critical > 0 && s <= critical)
				render_glyph_row(*p, y, ";1;31");
			else
				render_glyph_row(*p, y, "");
		for (i = 0; i < rest_x; i++)
			putchar(' ');
	}
	for (y = 0; y < rest_y; y++)
		for (i = 0; i < w.ws_col; i++)
			putchar(' ');
	fflush(stdout);
}

void
render_glyph_row(char c, int row, char *attrs)
{
	int i;
	unsigned char stripe = 0;

	if (row >= FONT_HEIGHT)
	{
		fprintf(stderr, "Error: row >= FONT_HEIGHT\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < FONT_CHARACTERS && font[i][0] != c; i++);
	stripe = font[i][row + 1];

	for (i = FONT_WIDTH - 1; i >= 0; i--)
		if (stripe & (1 << i))
			fprintf(stdout, "\033[7%sm \033[0m", attrs);
		else
			putchar(' ');
}

void
restore_cursor(void)
{
	fputs("\033[?25h", stdout);
}

void
restore_cursor_and_quit(int sig)
{
	restore_cursor();
	putchar('\n');
	exit(EXIT_SUCCESS);
}

void
wait_for_next_second(long int sync_fraction)
{
	struct timeval tv;
	struct timespec ts;

	if (gettimeofday(&tv, NULL) == -1)
	{
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}

	ts.tv_sec = 0;
	ts.tv_nsec = 1e9 - tv.tv_usec * 1e3 + sync_fraction;
	if (ts.tv_nsec >= 1e9)
	{
		ts.tv_sec = 1;
		ts.tv_nsec -= 1e9;
	}
	nanosleep(&ts, NULL);
}

int
main(int argc, char **argv)
{
	long int target = 0;
	long int start = 0;
	long int sync_fraction = 0;
	int critical = 10, blink = 0, opt;

	atexit(restore_cursor);
	signal(SIGINT, restore_cursor_and_quit);
	fputs("\033[?25l", stdout);

	while ((opt = getopt(argc, argv, "b:c:t:")) != -1)
	{
		switch (opt)
		{
			case 'b':
				blink = 2 * atoi(optarg);
				break;
			case 'c':
				critical = atoi(optarg);
				break;
			case 't':
				target = atol(optarg);
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}

	while (1)
	{
		if (target != 0)
		{
			if (countdown(target, critical))
			{
				if (blink == 0)
					exit(EXIT_SUCCESS);
				else
					full_color(&blink);
			}
		}
		else
			countup(&start, &sync_fraction);
		wait_for_next_second(sync_fraction);
	}

	/* not reached */
}
