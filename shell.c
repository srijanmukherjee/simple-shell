#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <linux/limits.h>

#include "util.h"
#include "commands.h"

#define PROMPT "$ "
#define MAX_LINE 80
#define READ_END 0
#define WRITE_END 1

typedef struct Child {
    int task_id;
    pid_t pid;
    struct Child * next;
} Child;

size_t sanitize_command(char * command);
void tokenize(char *command, char *args[]);
static void print_tokens(char * args[]);
static int run_command(char * args[]);
static void prompt();

int background_process_count = 0;
Child * children = NULL;

int main(int argc, char const *argv[], char **envp)
{
    setenv("SHELL", "myshell", 1);

    char command_buffer[MAX_LINE];
    char *args[MAX_LINE / 2 + 1] = {0};
    bool should_run = true;
    int last_status_code = -1;

    while (should_run)
    {
        // check if background process has exited
        Child *curr = children;
        Child *prev = NULL;
        int status;
        while (curr != NULL)
        {
            // WNOHANG returns immediately if process has not exited 
            if (waitpid(curr->pid, &status, WNOHANG))
            {
                background_process_count--;
                printf("[%d]  + %d done\n", curr->task_id, curr->pid);
                if (prev == NULL)   children = curr->next;
                else                prev->next = curr->next;
                free(curr);
                
                if (prev) curr = prev->next;
                else curr = children;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }

        prompt();
        fflush(stdout);
        fgets(command_buffer, MAX_LINE, stdin);

        size_t n = sanitize_command(command_buffer);
        if (n == 0) continue;

        if (strcmp(command_buffer, "!!") != 0)
        {
            tokenize(command_buffer, args);
        } 
        else if (args[0] == NULL)
        {
            fprintf(stderr, "No command in history!\n");
            continue;
        }
        else
        {
            for (int i = 0; args[i] != NULL; i++)
                printf("%s ", args[i]);
            printf("\n");
        }

        if (strcmp(args[0], "exit") == 0)
        {
            printf("Bye.\n");
            should_run = false;
            break;
        }

        else if (strcmp(args[0], "cd") == 0)
        {
            last_status_code = cd(args);
            continue;
        }

        last_status_code = run_command(args);
        fflush(stdout);
        #ifdef DEBUG
            printf("Exited with status code %d\n", last_status_code);
        #endif
    }
    
    // TODO: cleanup

    return 0;
}


size_t sanitize_command(char * command)
{
    size_t n = strlen(command);
    
    if (n == 0) return 0;

    // remove trailing newline
    if (command[n - 1] == '\n')
        command[--n] = '\0';

    return strip(command);
}

void tokenize(char *command, char *args[])
{
    char *token;
    token = strtok(command, " ");

    for (int i = 0; i < MAX_LINE / 2 + 1 && args[i] != NULL; i++)
    {
        free(args[i]);
        args[i] = NULL;
    }

    int i = 0;
    
    while (token != NULL)
    {
        args[i++] = strdup(token);
        token = strtok(NULL, " ");
    }

    args[i] = NULL;
}

static void print_tokens(char * args[])
{
    printf("tokens: { ");
    for (int i = 0; args[i] != NULL; i++)
    {
        if (i > 0) printf(", ");
        printf("%s", args[i]);
    }
    printf(" }\n");
}

static char * pwd()
{
    static char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    return cwd;
}

static void prompt()
{
    printf("[%s]", pwd());
    printf(PROMPT);
}

static int run_command(char * args[])
{
    if (args[0] == NULL) return 1;

    size_t argc = 0;
    while (args[argc] != NULL) argc++;

    // check for redirection
    int input_redirection_pos = -1;
    int output_redirection_pos = -1;
    int pipe_pos = -1;
    bool multiple_redirections = false;
    bool multiple_pipes = false;
    FILE *fp = NULL;

    for (int i = 0; i < argc && !multiple_redirections && !multiple_pipes; i++)
    {
        if (strcmp(args[i], "<") == 0)
        {
            if (input_redirection_pos > -1 || output_redirection_pos > -1 || pipe_pos > -1) multiple_redirections = true;
            input_redirection_pos = i;
        }

        else if (strcmp(args[i], ">") == 0)
        {
            if (input_redirection_pos > -1 || output_redirection_pos > -1 || pipe_pos > -1) multiple_redirections = true;
            output_redirection_pos = i;
        }

        else if (strcmp(args[i], "|") == 0)
        {
            if (pipe_pos > -1 || input_redirection_pos > -1 || output_redirection_pos > -1) multiple_pipes = true;
            pipe_pos = i;
        }
    }

    if (multiple_redirections == true)
    {
        fprintf(stderr, "multiple redirections not supported\n");
        return 1;
    }

    if (multiple_pipes == true)
    {
        fprintf(stderr, "multiple pipes not supported\n");
        return 1;
    }

    if (output_redirection_pos > -1)
    {
        if (output_redirection_pos != argc - 2)
        {
            fprintf(stderr, "> is supported at the second last position only\n");
            return 1;
        }

        fp = fopen(args[argc - 1], "w+");
    }

    if (input_redirection_pos > -1)
    {
        if (input_redirection_pos != argc - 2)
        {
            fprintf(stderr, "< is supported at the second last position only\n");
            return 1;
        }

        fp = fopen(args[argc - 1], "r");

        if (fp == NULL)
        {
            perror(args[argc - 1]);
            return 1;
        }
    }

    if (pipe_pos > -1 && pipe_pos == argc - 1)
    {
        fprintf(stderr, "expected command after pipe\n");
        return 1;
    }

    if (pipe_pos > -1 && strcmp(args[argc - 1], "&") == 0)
    {
        fprintf(stderr, "piped background processes are not supported\n");
        return 1;
    }

    int fd[2];

    if (pipe_pos > -1)
    {
        if (pipe(fd) < 0)
        {
            fprintf(stderr, "Pipe failed\n");
            return 1;
        }
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Fork failed\n");
        return 1;
    }

    if (pid == 0)
    {
        if (strcmp(args[argc - 1], "&") == 0)
        {
            args[argc - 1] = NULL;
        }

        if (pipe_pos > -1)
        {
            dup2(fd[WRITE_END], STDOUT_FILENO);
            close(fd[READ_END]);
            close(fd[WRITE_END]);
            args[pipe_pos] = NULL;
        }

        if (output_redirection_pos > -1)
        {
            args[output_redirection_pos] = NULL;
            if (fp != NULL)
                dup2(fileno(fp), STDOUT_FILENO);
        }

        if (input_redirection_pos > -1)
        {
            args[input_redirection_pos] = NULL;
            if (fp != NULL)
                dup2(fileno(fp), STDIN_FILENO);
        }

        if (execvp(args[0], args) < 0)
        {
            exit(errno);
        }
    }
    else
    {
        if (strcmp(args[argc - 1], "&") == 0)
        {
            printf("[%d] %d\n", ++background_process_count, pid);

            Child *child = malloc(sizeof(Child));
            child->task_id = background_process_count;
            child->pid = pid;
            child->next = children;
            children = child;
        }
        else
        {
            if (pipe_pos > -1)
            {
                pid_t subcommand_pid = fork();

                if (subcommand_pid < 0)
                {
                    fprintf(stderr, "Fork failed\n");
                }

                else if (subcommand_pid == 0)
                {
                    dup2(fd[READ_END], STDIN_FILENO);
                    close(fd[READ_END]);
                    close(fd[WRITE_END]);

                    if (execvp(args[pipe_pos + 1], args + pipe_pos + 1) < 0)
                    {
                        exit(errno);
                    }
                } else {
                    int status;
                    close(fd[READ_END]);
                    close(fd[WRITE_END]);
                    waitpid(subcommand_pid, &status, 0);
                    if (fp != NULL) fclose(fp);
                    
                    if (WIFEXITED(status))
                        return WEXITSTATUS(status);
                }
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (fp != NULL) fclose(fp);
                
                if (WIFEXITED(status))
                    return WEXITSTATUS(status);
            }
        }
    }

    return 0;
}