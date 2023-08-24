
#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "history.h"
#include "logger.h"
#include "ui.h"

bool success = true;

void exit_sequence(int signo)
{
    hist_destroy();
    destroy_ui();
    exit(signo);
}

char** tok(char* str, char* delim)
{
    for (int i = 0; i < strlen(str); i++) if (str[i] == '\t' || str[i] == '\r' || str[i] == '\n') str[i] = ' ';
    int len = 0;
    for (int i = 0; i < strlen(delim); i++)
    {
        for (int j = 0; j < strlen(str); j++) if (str[j] == delim[i]) len++;
    }
    char** ret = malloc((len+2) * sizeof(char*));

    int i = 0;
    char* t;
    while ((t = strsep(&str, delim)))
    {
        if (t[0] == 0) continue;
        ret[i++] = strdup(t);
    }

    ret[i] = (char*) NULL;
    return ret;
}

void destruct_cpp(char** cpp)
{
    int i = 0;
    while (cpp[i]) free(cpp[i++]);
    free(cpp);
}

void cmd_master(char*);
void cmd_bang(char*);
void cmd_cd(char*);
void cmd_hs(void);
void cmd_run(char*);

int main(void)
{
    signal(SIGINT, exit_sequence);
    init_ui();
    hist_init(100);

    if (scripting)
    {
        size_t buf_sz = 0, read_sz;
        char* command = NULL;
        while ((read_sz = getline(&command, &buf_sz, stdin)) >= 0)
	{
            if (command == NULL) exit_sequence(0);
            if (memcmp(command, "exit", 4) == 0) { free(command); exit_sequence(0); }
            if (command[0] == 0) { success = false; free(command); continue; }

            cmd_master(command);

            printf("\n");
            free(command);
            command = NULL;
	}

        exit_sequence(0);
    }

    //else interactive mode
    while (true)
    {
        char* command = read_command();

        if (command == NULL) exit_sequence(0);
        if (strcmp(command, "exit") == 0) { free(command); exit_sequence(0); }
        if (command[0] == 0) { success = false; free(command); continue; }

        cmd_master(command);

        printf("\n");
        free(command);
    }

    exit_sequence(0);
}

void cmd_master(char* command)
{
    char* copy = strdup(command);
    command[strcspn(command, "#")] = 0;

    if (command[0] == '!')                  { free(copy); cmd_bang(command); return; }
    
    if (memcmp(command, "history", 7) == 0)   cmd_hs();
    else if (memcmp(command, "cd", 2) == 0)   cmd_cd(command + 2);
    else                                      cmd_run(command);

    if (success) hist_add(copy);
    free(copy);
}

void bang_fail(char* str)
{
    success = false;
    printf("crash: command not found in history: %s\n", str);
}

void cmd_bang(char* str)
{
    if (str[1] == 0) { success = false; return; }

    char* cmd;
    if (str[1] == '!')
    {
        if (hist_search_cnum(hist_last_cnum()) == NULL) { bang_fail(str); return; }
        cmd = strdup(hist_search_cnum(hist_last_cnum()));
    }
    else if (str[1] < 58 && str[1] > 47)
    {
        if (hist_search_cnum(atoi(str+1)) == NULL) { bang_fail(str); return; }
        cmd = strdup(hist_search_cnum(atoi(str+1)));
    }
    else
    {
        if (hist_search_prefix(str+1) == NULL) { bang_fail(str); return; }
        cmd = strdup(hist_search_prefix(str+1));
    }
    
    //LOG("bang result: %s\n", cmd);
    cmd_master(cmd);
    free(cmd);
}

void cmd_cd(char* command)
{
    if (command[0] == ' ') command++;
    
    int status;
    if (command[0] == 0 || (command[0] == '~' && command[1] == 0))
    {
        status = chdir(getenv("HOME"));
    }
    else
    {
        char* cwd = malloc(256); getcwd(cwd, 256);
        char* new_dir = malloc(strlen(cwd) + strlen(command) + 2);
        sprintf(new_dir, "%s/%s", cwd, command);
        free(cwd);

        status = chdir(new_dir);
        free(new_dir);
    }
    success = status == 0;
    if (success == false) printf("chdir: no such file or directory: %s\n", command);
}

void cmd_hs()
{
    hist_print();
    success = true;
}

void success_set(bool b) { success = b; }

struct command_line {
    char**  tokens;
    char    io_symbol;
};

struct command_line* cl_create(char** toks, char sym)
{
    struct command_line* cl = malloc(sizeof(struct command_line));
    cl->tokens = toks;
    cl->io_symbol = sym;
    return cl;
}

void cl_destroy(struct command_line* cl)
{
    int i = 0;
    while (cl->tokens[i]) { free(cl->tokens[i]); i++; }
    free(cl->tokens);
    free(cl);
}

void execute_pipeline(struct command_line** cmds);

void cmd_run(char* str)
{
    char** str_blobs = tok(str, "|<>");

    int i = 0;
    while (str_blobs[i]) i++;
    int cls_len = 2 + i/2;

    struct command_line** cmd_lines  = malloc(cls_len * sizeof(struct command_line*));
    char**                io_symbols = malloc(cls_len * sizeof(char*));

    i = 0; int smi = 0; int cli = 0; int sym_prev = 0;
    while (str_blobs[i])
    {
        if (str_blobs[i][0] == '|' || str_blobs[i][0] == '<' || str_blobs[i][0] == '>')
        {
            if (sym_prev)
            {
                io_symbols[smi-1] = realloc(io_symbols[smi-1], strlen(io_symbols[smi-1]) + strlen(str_blobs[i]) + 1);
                strcat(io_symbols[smi-1], str_blobs[i]);
            }
	    else
                io_symbols[smi++] = str_blobs[i];
            sym_prev = 1;
        }
	else
        {
            cmd_lines[cli++] = cl_create(tok(str_blobs[i], " "), 0);
            sym_prev = 0;
        }

        i++;
    }
    io_symbols[smi] = NULL; cmd_lines[cli] = NULL;
    destruct_cpp(str_blobs);

    i = 0;
    while (cmd_lines[i])
    {
        if (io_symbols[i] == NULL) break;
	
        if      (io_symbols[i][0] == '|') cmd_lines[i]->io_symbol = '|';
        else if (io_symbols[i][0] == '>')
        {
            cmd_lines[i]->io_symbol = '|';
	    cmd_lines[i+1]->io_symbol = (io_symbols[i][1] == '>') ? '&' : '>';
	    break;
        }
        else  //(io_symbols[i][0] == '<')
        {
            struct command_line* swap = cmd_lines[i];
            cmd_lines[i] = cmd_lines[i+1]; cmd_lines[i+1] = swap;
            cmd_lines[i]->io_symbol = '<';
        }

	i++;
    }
    destruct_cpp(io_symbols);

    i = 0;
    while (cmd_lines[i])
    {
        if (cmd_lines[i]->io_symbol == 0) break;
        if (cmd_lines[i]->io_symbol == '|') { i++; continue; }

        if (cmd_lines[i]->io_symbol == '<')
        {
            cmd_lines[i]->tokens = realloc(cmd_lines[i]->tokens, 3 * sizeof(char*));
            cmd_lines[i]->tokens[2] = (char*) NULL;
            cmd_lines[i]->tokens[1] = cmd_lines[i]->tokens[0];
            cmd_lines[i]->tokens[0] = strdup("cat");
            cmd_lines[i]->io_symbol = '|';
        }
        else //(cmd_lines[i]->io_symbol == '>' or '&')
        {
            int ap = (cmd_lines[i]->io_symbol == '&');
            int toks = 0; while (cmd_lines[i]->tokens[toks]) toks++;
            cmd_lines[i]->tokens = realloc(cmd_lines[i]->tokens, (2 + ap + toks) * sizeof(char*));

            for (int j = 1 + ap + toks; j > ap; j--)
                cmd_lines[i]->tokens[j] = cmd_lines[i]->tokens[j-1];
	    cmd_lines[i]->tokens[0] = strdup("tee");
            if (ap) cmd_lines[i]->tokens[1] = strdup("-a");
            cmd_lines[i]->io_symbol = '>'; //hopefully i can dup2 stdout to pipe and just toss it
            break;
        }

        i++;
    }

    success = true;
    int stdout_save = dup(1);
    int stdin_save = dup(0);

    execute_pipeline(cmd_lines);
    
    dup2(stdout_save, 1); close(stdout_save);
    dup2(stdin_save, 0); close(stdin_save);
    
    i = 0; while (cmd_lines[i]) cl_destroy(cmd_lines[i++]);
    free(cmd_lines);
}

void execute_pipeline(struct command_line** cmds)
{
    int pa[2];
    pipe(pa);
    int p_read = *pa;
    int p_write = *(pa+1);
    
    pid_t pid = fork();
    if (pid == 0)
    {   //child
        close(p_read);
        if ((*cmds)->io_symbol) dup2(p_write, STDOUT_FILENO);
        if (execvp((*cmds)->tokens[0], (*cmds)->tokens) == -1)
        {
            success_set(false);
	    if (scripting) fclose(stdin);
            printf("crash: no such file or directory: %s\n", (*cmds)->tokens[0]);
	    LOG("pid: %d\n", getpid());
        }
    }
    else
    {   //parent
        close(p_write);
        dup2(p_read, STDIN_FILENO);
        int status;
        wait(&status);
	if (success && (*cmds)->io_symbol == '|') execute_pipeline(cmds+1);
    }
}
