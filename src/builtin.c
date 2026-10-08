#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <time.h>

#include "builtin.h"


/* =========================================================
   BUILTIN: cd
   ========================================================= */

static int builtin_cd(command_t *cmd)
{
    const char *directory;

    if (cmd->argc == 1)
    {
        directory = getenv("HOME");

        if (directory == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return -1;
        }
    }
    else if (cmd->argc == 2)
    {
        directory = cmd->argv[1];
    }
    else
    {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (chdir(directory) != 0)
    {
        perror("cd");
        return -1;
    }

    return 0;
}


/* =========================================================
   BUILTIN: pwd
   ========================================================= */

static int builtin_pwd(command_t *cmd)
{
    char current_directory[4096];

    if (cmd->argc > 1)
    {
        fprintf(stderr, "pwd: too many arguments\n");
        return -1;
    }

    if (getcwd(current_directory,
               sizeof(current_directory)) == NULL)
    {
        perror("pwd");
        return -1;
    }

    printf("%s\n", current_directory);

    return 0;
}


/* =========================================================
   BUILTIN: echo
   ========================================================= */

static int builtin_echo(command_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++)
    {
        printf("%s", cmd->argv[i]);

        if (i < cmd->argc - 1)
        {
            printf(" ");
        }
    }

    printf("\n");

    return 0;
}


/* =========================================================
   BUILTIN: exit
   ========================================================= */

static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr,
                "exit: too many arguments\n");

        return -1;
    }

    return 1;
}


/* =========================================================
   BUILTIN: mkcd
   Create directory and enter it
   ========================================================= */

static int builtin_mkcd(command_t *cmd)
{
    if (cmd->argc != 2)
    {
        fprintf(stderr,
                "mkcd: usage: mkcd <directory>\n");

        return -1;
    }

    if (mkdir(cmd->argv[1], 0755) != 0)
    {
        perror("mkcd");

        return -1;
    }

    if (chdir(cmd->argv[1]) != 0)
    {
        perror("mkcd: chdir");

        return -1;
    }

    printf("Created directory and entered: %s\n",
           cmd->argv[1]);

    return 0;
}


/* =========================================================
   BUILTIN: croot
   Return to Shellforge project root
   ========================================================= */

static int builtin_croot(command_t *cmd)
{
    char current[4096];
    char test_path[8192];

    if (cmd->argc != 1)
    {
        fprintf(stderr,
                "croot: no arguments expected\n");

        return -1;
    }

    /*
     * Start from the current directory.
     */
    if (getcwd(current, sizeof(current)) == NULL)
    {
        perror("croot");
        return -1;
    }

    /*
     * Move upward until we find the Shellforge
     * executable.
     */
    while (1)
    {
        snprintf(test_path,
                 sizeof(test_path),
                 "%s/shellforge",
                 current);

        /*
         * Check whether the shell executable exists.
         */
        if (access(test_path, X_OK) == 0)
        {
            if (chdir(current) != 0)
            {
                perror("croot");
                return -1;
            }

            printf("Returned to Shellforge root: %s\n",
                   current);

            return 0;
        }

        /*
         * Find the last '/'.
         */
        char *last_slash = strrchr(current, '/');

        if (last_slash == NULL ||
            last_slash == current)
        {
            break;
        }

        /*
         * Remove the last directory.
         */
        *last_slash = '\0';
    }

    fprintf(stderr,
            "croot: Shellforge root not found\n");

    return -1;
}

/* =========================================================
   BUILTIN: count
   Count files and directories
   ========================================================= */

static int builtin_count(command_t *cmd)
{
    DIR *directory;
    struct dirent *entry;

    int files = 0;
    int directories = 0;

    if (cmd->argc != 1)
    {
        fprintf(stderr,
                "count: no arguments expected\n");

        return -1;
    }

    directory = opendir(".");

    if (directory == NULL)
    {
        perror("count");
        return -1;
    }

    while ((entry = readdir(directory)) != NULL)
    {
        struct stat file_info;

        /*
         * Ignore . and ..
         */
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        /*
         * Get information about the entry.
         */
        if (stat(entry->d_name, &file_info) != 0)
        {
            continue;
        }

        if (S_ISDIR(file_info.st_mode))
        {
            directories++;
        }
        else if (S_ISREG(file_info.st_mode))
        {
            files++;
        }
    }

    closedir(directory);

    printf("Files       : %d\n", files);
    printf("Directories : %d\n", directories);

    return 0;
}


/* =========================================================
   BUILTIN: recent
   Show recently modified files
   ========================================================= */

static int builtin_recent(command_t *cmd)
{
    DIR *directory;
    struct dirent *entry;

    if (cmd->argc != 1)
    {
        fprintf(stderr,
                "recent: no arguments expected\n");

        return -1;
    }

    directory = opendir(".");

    if (directory == NULL)
    {
        perror("recent");

        return -1;
    }

    printf("Recently modified files:\n");

    /*
     * Display up to 10 files.
     */
    int count = 0;

    while ((entry = readdir(directory)) != NULL &&
           count < 10)
    {
        struct stat file_info;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (stat(entry->d_name, &file_info) != 0)
        {
            continue;
        }

        if (S_ISREG(file_info.st_mode))
        {
            char time_buffer[80];

            struct tm *time_info =
                localtime(&file_info.st_mtime);

            strftime(time_buffer,
                     sizeof(time_buffer),
                     "%Y-%m-%d %H:%M:%S",
                     time_info);

            printf("%d. %-25s %s\n",
                   count + 1,
                   entry->d_name,
                   time_buffer);

            count++;
        }
    }

    closedir(directory);

    if (count == 0)
    {
        printf("No files found.\n");
    }

    return 0;
}


/* =========================================================
   BUILTIN: sysinfo
   Display system information
   ========================================================= */

static int builtin_sysinfo(command_t *cmd)
{
    struct utsname system_info;

    if (cmd->argc != 1)
    {
        fprintf(stderr,
                "sysinfo: no arguments expected\n");

        return -1;
    }

    if (uname(&system_info) != 0)
    {
        perror("sysinfo");

        return -1;
    }

    const char *username = getenv("USER");

    if (username == NULL)
    {
        username = "unknown";
    }

    printf("\n");
    printf("=================================\n");
    printf("        SYSTEM INFORMATION\n");
    printf("=================================\n");
    printf("OS       : %s\n", system_info.sysname);
    printf("Hostname : %s\n", system_info.nodename);
    printf("Kernel   : %s\n", system_info.release);
    printf("Machine  : %s\n", system_info.machine);
    printf("User     : %s\n", username);
    printf("=================================\n");
    printf("\n");

    return 0;
}


/* =========================================================
   CHECK WHETHER COMMAND IS A BUILTIN
   ========================================================= */

int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
    {
        return 0;
    }

    if (strcmp(cmd->argv[0], "cd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "pwd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "echo") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "exit") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "mkcd") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "croot") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "count") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "recent") == 0)
        return 1;

    if (strcmp(cmd->argv[0], "sysinfo") == 0)
        return 1;

    return 0;
}


/* =========================================================
   EXECUTE BUILTIN
   ========================================================= */

int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
    {
        return -1;
    }

    if (strcmp(cmd->argv[0], "cd") == 0)
    {
        return builtin_cd(cmd);
    }

    if (strcmp(cmd->argv[0], "pwd") == 0)
    {
        return builtin_pwd(cmd);
    }

    if (strcmp(cmd->argv[0], "echo") == 0)
    {
        return builtin_echo(cmd);
    }

    if (strcmp(cmd->argv[0], "exit") == 0)
    {
        return builtin_exit(cmd);
    }

    if (strcmp(cmd->argv[0], "mkcd") == 0)
    {
        return builtin_mkcd(cmd);
    }

    if (strcmp(cmd->argv[0], "croot") == 0)
    {
        return builtin_croot(cmd);
    }

    if (strcmp(cmd->argv[0], "count") == 0)
    {
        return builtin_count(cmd);
    }

    if (strcmp(cmd->argv[0], "recent") == 0)
    {
        return builtin_recent(cmd);
    }

    if (strcmp(cmd->argv[0], "sysinfo") == 0)
    {
        return builtin_sysinfo(cmd);
    }

    return -1;
}
