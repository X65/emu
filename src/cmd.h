#define MAX_ARGS 10

struct cmd_arguments {
    struct {
        int key;
        const char* arg;
    } args[MAX_ARGS];
    unsigned argc;
    unsigned arg_num;
};

typedef void (*cmd_callback_t)(const char* name, const struct cmd_arguments* args);

void cmd_init(void);
void cmd_parse_line(const char* line);
void cmd_cleanup(void);
