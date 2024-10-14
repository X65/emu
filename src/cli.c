#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "cli.h"

static struct termios ttysave;

void cli_init() {
    tcgetattr(STDIN_FILENO, &ttysave);
    // do not wait for ENTER
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~(ICANON | ECHO);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tty);

    // set STDIN to non-blocking I/O
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

void cli_cleanup() {
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttysave);
}

void cli_update() {
    char buf[5];
    int n = read(STDIN_FILENO, buf, 4);
    if (n > 0) {
        printf("%.*s\n", n, buf);
        fflush(stdout);
    }
}
