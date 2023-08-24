/**
 * @file
 *
 * Text-based UI functionality. These functions are primarily concerned with
 * interacting with the readline library.
 */

#ifndef _UI_H_
#define _UI_H_

extern bool success;
extern bool scripting;

void init_ui(void);
void destroy_ui(void);

char* prompt_line(void);
char* prompt_username(void);
char* prompt_hostname(void);
char* prompt_cwd(void);
int prompt_status(void);
unsigned int prompt_cmd_num(void);

char* read_command(void);

#endif
