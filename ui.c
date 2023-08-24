
#include <stdio.h>
#include <readline/readline.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include "history.h"
#include "logger.h"
#include "ui.h"

static const char* good_str = "+";
static const char* bad_str  = "-";
static char* locale;
bool scripting = false;

static int readline_init(void);

void init_ui(void)
{
    //LOGP("Initializing UI...\n");

    locale = setlocale(LC_ALL, "en_US.UTF-8");
    //LOG("Setting locale: %s\n", (locale != NULL) ? locale : "could not set locale!");

    rl_startup_hook = readline_init;

    if (!isatty(STDIN_FILENO))
    {
        //LOGP("data piped in on STDIN; entering script mode\n");
	scripting = true;
    }
}

void destroy_ui(void)
{
    free(locale);
}

char* prompt_line(void)
{
    const char* status = prompt_status() ? good_str : bad_str;

    char cmd_num[25];
    snprintf(cmd_num, 25, "%u", prompt_cmd_num());

    char* user = prompt_username();
    char* host = prompt_hostname();
    char* cwd = prompt_cwd();

    char* format_str = ">>-[%s]-[%s]-[%s@%s:%s]-> ";

    size_t prompt_sz
        = strlen(format_str)
        + strlen(status)
        + strlen(cmd_num)
        + strlen(user)
        + strlen(host)
        + strlen(cwd)
        + 1;

    char* prompt_str =  malloc(sizeof(char) * prompt_sz);

    snprintf(prompt_str, prompt_sz, format_str,
            status,
            cmd_num,
            user,
            host,
            cwd);

    return prompt_str;
}

char* prompt_username(void)
{
    char* username = getenv("USER");
    if (username) return username;
    return "unknown_user";
}

char* prompt_hostname(void)
{
    char* hostname = malloc(128);
    if (gethostname(hostname, 127) == 0) return hostname;
    return "unknown_host";
}

char* prompt_cwd(void)
{
    char* cwd = malloc(256); getcwd(cwd, 256); if (!cwd) return "/unknown/path";
    char* home = getenv("HOME");
    char* dir_str;

    if (home && (memcmp(cwd, home, strlen(home)) == 0))
    {
        dir_str = malloc(strlen(cwd) - strlen(home) + 2);
        sprintf(dir_str, "~%s", cwd + strlen(home));
    }
    else
    {
        dir_str = malloc(strlen(cwd) + 1);
	strcpy(dir_str, cwd);
    }

    free(cwd);
    return dir_str;
}

int prompt_status(void)
{
    return success;
}

unsigned int prompt_cmd_num(void)
{
    return hist_last_cnum() + 1;
}

char* read_command(void)
{
    char* command = NULL;
    char* prompt = prompt_line();
    command = readline(prompt);
    free(prompt);
    return command;
}

int readline_init(void)
{
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    return 0;
}
