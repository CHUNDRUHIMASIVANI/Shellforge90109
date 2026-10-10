#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

/*
 * Check whether a command is a built-in command.
 *
 * Returns:
 *      1 -> built-in
 *      0 -> external command
 */
int is_builtin(const command_t *cmd);

/*
 * Execute a built-in command.
 *
 * Existing built-ins:
 *      cd
 *      pwd
 *      echo
 *      exit
 *
 * Custom built-ins:
 *      mkcd
 *      croot
 *      count
 *      recent
 *      sysinfo
 *
 * Job-control built-ins:
 *      jobs
 *      fg
 *      bg
 *
 * Returns:
 *      0  -> command executed successfully
 *      1  -> shell should exit
 *     -1  -> error
 */
int execute_builtin(command_t *cmd);

/*
 * Job-control built-ins.
 */

/* Display background jobs. */
int builtin_jobs(command_t *cmd);

/* Bring a background or stopped job to the foreground. */
int builtin_fg(command_t *cmd);

/* Resume a stopped job in the background. */
int builtin_bg(command_t *cmd);

#endif
