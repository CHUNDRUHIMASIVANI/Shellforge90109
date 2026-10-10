#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "jobs.h"
#include "builtin.h"
#include "job_control.h"


/* =========================================================
   BUILTIN: cd
   Change current working directory
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
   Display current working directory
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
   Display arguments
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
   Terminate shell
   ========================================================= */

static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 1)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }

    return 1;
}


/* =========================================================
   BUILTIN: mkcd
   Create a directory and enter it
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
   Return to Shellforge root
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

    if (getcwd(current, sizeof(current)) == NULL)
    {
        perror("croot");
        return -1;
    }

    while (1)
    {
        int length = snprintf(test_path,
                              sizeof(test_path),
                              "%s/shellforge",
                              current);

        if (length < 0 ||
            (size_t)length >= sizeof(test_path))
        {
            fprintf(stderr, "croot: path too long\n");
            return -1;
        }

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

        char *last_slash = strrchr(current, '/');

        if (last_slash == NULL)
        {
            break;
        }

        if (last_slash == current)
        {
            break;
        }

        *last_slash = '\0';
    }

    fprintf(stderr,
            "croot: Shellforge root not found\n");

    return -1;
}


/* =========================================================
   BUILTIN: count
   Count regular files and directories
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

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

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
   Display recently modified regular files
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

    /*
     * Store up to 10 regular files and sort them
     * by modification time, newest first.
     */

    struct recent_file
    {
        char name[4096];
        time_t modified;
    };

    struct recent_file files[10];
    int count = 0;

    while ((entry = readdir(directory)) != NULL)
    {
        struct stat info;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (stat(entry->d_name, &info) != 0 ||
            !S_ISREG(info.st_mode))
        {
            continue;
        }

        int position;

        if (count < 10)
        {
            position = count++;
        }
        else
        {
            if (info.st_mtime <= files[count - 1].modified)
            {
                continue;
            }

            position = count - 1;
        }

        while (position > 0 &&
               files[position - 1].modified < info.st_mtime)
        {
            files[position] = files[position - 1];
            position--;
        }

        snprintf(files[position].name,
                 sizeof(files[position].name),
                 "%s",
                 entry->d_name);

        files[position].modified = info.st_mtime;
    }

    closedir(directory);

    printf("Recently modified files:\n");

    if (count == 0)
    {
        printf("No files found.\n");
        return 0;
    }

    for (int i = 0; i < count; i++)
    {
        char time_buffer[80];
        struct tm time_info;

        if (localtime_r(&files[i].modified, &time_info) == NULL)
        {
            snprintf(time_buffer,
                     sizeof(time_buffer),
                     "Unknown time");
        }
        else
        {
            strftime(time_buffer,
                     sizeof(time_buffer),
                     "%Y-%m-%d %H:%M:%S",
                     &time_info);
        }

        printf("%d. %-25s %s\n",
               i + 1,
               files[i].name,
               time_buffer);
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
    printf("=================================\n\n");

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

    const char *name = cmd->argv[0];

    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "echo") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "mkcd") == 0 ||
           strcmp(name, "croot") == 0 ||
           strcmp(name, "count") == 0 ||
           strcmp(name, "recent") == 0 ||
           strcmp(name, "sysinfo") == 0 ||
           strcmp(name, "jobs") == 0 ||
           strcmp(name, "fg") == 0 ||
           strcmp(name, "bg") == 0;
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

    const char *name = cmd->argv[0];

    if (strcmp(name, "cd") == 0)
        return builtin_cd(cmd);

    if (strcmp(name, "pwd") == 0)
        return builtin_pwd(cmd);

    if (strcmp(name, "echo") == 0)
        return builtin_echo(cmd);

    if (strcmp(name, "exit") == 0)
        return builtin_exit(cmd);

    if (strcmp(name, "mkcd") == 0)
        return builtin_mkcd(cmd);

    if (strcmp(name, "croot") == 0)
        return builtin_croot(cmd);

    if (strcmp(name, "count") == 0)
        return builtin_count(cmd);

    if (strcmp(name, "recent") == 0)
        return builtin_recent(cmd);

    if (strcmp(name, "sysinfo") == 0)
        return builtin_sysinfo(cmd);

    if (strcmp(name, "jobs") == 0)
        return builtin_jobs(cmd);

    if (strcmp(name, "fg") == 0)
        return builtin_fg(cmd);

    if (strcmp(name, "bg") == 0)
        return builtin_bg(cmd);

    return -1;
}


/* =========================================================
   BUILTIN: jobs
   Display background/stopped jobs
   ========================================================= */

int builtin_jobs(command_t *cmd)
{
    if (cmd->argc != 1)
    {
        fprintf(stderr, "jobs: no arguments expected\n");
        return -1;
    }

    jobs_print();

    return 0;
}


/* =========================================================
   BUILTIN: fg
   Bring a job into the foreground
   ========================================================= */

int builtin_fg(command_t *cmd)
{
    int job_id;
    job_t *job;
    int status;

    if (cmd->argc != 2)
    {
        fprintf(stderr, "fg: usage: fg <job number>\n");
        return -1;
    }

    char *end;
    errno = 0;

    long parsed_id = strtol(cmd->argv[1], &end, 10);

    if (errno != 0 ||
        end == cmd->argv[1] ||
        *end != '\0' ||
        parsed_id <= 0 ||
        parsed_id > 2147483647L)
    {
        fprintf(stderr, "fg: invalid job number\n");
        return -1;
    }

    job_id = (int)parsed_id;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "fg: no such job: %d\n", job_id);
        return -1;
    }

    give_terminal_to(job->pgid);

    if (job->state == JOB_STOPPED)
    {
        if (kill(-job->pgid, SIGCONT) == -1)
        {
            perror("fg: SIGCONT");
            take_terminal_back();
            return -1;
        }
    }

    job_continue(job->pgid);

    while (1)
    {
        pid_t result = waitpid(-job->pgid,
                               &status,
                               WUNTRACED);

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            if (errno != ECHILD)
                perror("fg: waitpid");

            break;
        }

        if (WIFSTOPPED(status))
        {
            job_stop(job->pgid);
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status))
        {
            job_done(job->pgid);
            break;
        }
    }

    take_terminal_back();

    if (job->state == JOB_DONE)
    {
        job_remove(job_id);
    }

    return 0;
}


/* =========================================================
   BUILTIN: bg
   Resume a stopped job in the background
   ========================================================= */

int builtin_bg(command_t *cmd)
{
    int job_id;
    job_t *job;

    if (cmd->argc != 2)
    {
        fprintf(stderr, "bg: usage: bg <job number>\n");
        return -1;
    }

    char *end;
    errno = 0;

    long parsed_id = strtol(cmd->argv[1], &end, 10);

    if (errno != 0 ||
        end == cmd->argv[1] ||
        *end != '\0' ||
        parsed_id <= 0 ||
        parsed_id > 2147483647L)
    {
        fprintf(stderr, "bg: invalid job number\n");
        return -1;
    }

    job_id = (int)parsed_id;

    job = job_find(job_id);

    if (job == NULL)
    {
        fprintf(stderr, "bg: job not found: %d\n", job_id);
        return -1;
    }

    if (kill(-job->pgid, SIGCONT) == -1)
    {
        perror("bg: kill");
        return -1;
    }

    job_continue(job->pgid);

    printf("[%d] %s &\n",
           job->job_id,
           job->command);

    return 0;
}
