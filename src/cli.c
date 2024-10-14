#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <locale.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "sokol_app.h"

#include "cli.h"
#include "cmd.h"

static struct termios ttysave;

static bool sigwinch_received = false;
static const char* prompt = "> ";
static void cb_linehandler(char*);
static void sighandler(int);

void cli_init(void) {
    tcgetattr(STDIN_FILENO, &ttysave);
    // do not wait for ENTER
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~(ICANON) | ECHO;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tty);

    // set STDIN to non-blocking I/O
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    // prepare readline
    setlocale(LC_ALL, "");         // set the default locale values according to environment variables.
    signal(SIGWINCH, sighandler);  // handle window size changes
    rl_callback_handler_install(prompt, cb_linehandler);
}

void cli_cleanup(void) {
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttysave);
    rl_callback_sigcleanup();
    signal(SIGWINCH, NULL);
}

void cli_update(void) {
    if (sigwinch_received) {
        rl_resize_terminal();
        sigwinch_received = false;
    }

    int bytes_available;
    if (ioctl(STDIN_FILENO, FIONREAD, &bytes_available) == 0 && bytes_available) {
        rl_callback_read_char();
    }
}

static bool is_empty(const char* s) {
    while (*s != '\0') {
        if (!isspace(*s)) return false;
        ++s;
    }
    return true;
}

static void sighandler(int sig) {
    if (sig == SIGWINCH) {
        sigwinch_received = true;
    }
}

static void cb_linehandler(char* line) {
    /* Can use ^D (stty eof) or `quit' to quit. */
    if (!line || strcmp(line, "quit") == 0) {
        if (!line) printf("\n");
        /* This function needs to be called to reset the terminal settings,
           and calling it from the line handler keeps one extra prompt from
           being displayed. */
        rl_callback_handler_remove();

        sapp_request_quit();
    }
    else {
        if (!is_empty(line)) {
            add_history(line);
            cmd_parse_line(line);
        }
        free(line);
    }
}
