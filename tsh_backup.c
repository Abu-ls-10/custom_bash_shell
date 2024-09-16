/* 
 * tsh - A tiny shell program with job control
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* Per-job data */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, FG, BG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

volatile sig_atomic_t ready; /* Is the newest child in its own process group? */

/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);

int builtin_cmd(char **argv);

void do_bgfg(char **argv);

void waitfg(pid_t pid);

void sigchld_handler(int sig);

void sigint_handler(int sig);

void sigtstp_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);

void sigquit_handler(int sig);

void sigusr1_handler(int sig);

void clearjob(struct job_t *job);

void initjobs(struct job_t *jobs);

int freejid(struct job_t *jobs);

int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);

int deletejob(struct job_t *jobs, pid_t pid);

pid_t fgpid(struct job_t *jobs);

struct job_t *getjobpid(struct job_t *jobs, pid_t pid);

struct job_t *getjobjid(struct job_t *jobs, int jid);

int pid2jid(pid_t pid);

void listjobs(struct job_t *jobs);

void usage(void);

void unix_error(char *msg);

void app_error(char *msg);

typedef void handler_t(int);

handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) {
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(STDOUT_FILENO, STDERR_FILENO);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != -1) {
        switch (c) {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    Signal(SIGUSR1, sigusr1_handler); /* Child is ready */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }


        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) {
    char buf[MAXLINE];
    char *arguments[MAXARGS];
    char *child_args[MAXARGS];
    int size_of_arg = 0;

    // Parse the command line
    strcpy(buf, cmdline);
    size_of_arg = parseline(buf, arguments);

    // Ignore empty lines
    if (arguments[0] == NULL) {
        return;
    }

    int in_redir = 0, out_redir = 0;
    char *in_file = NULL, *out_file = NULL;

    int j = 0;

    for (int i = 0; i < size_of_arg; i++) {
        if (strcmp(arguments[i], ">") == 0) {
            out_redir = 1;
            out_file = arguments[i + 1];
            i++; // skip the name of the file
            break;
        } else if (strcmp(arguments[i], "<") == 0) {
            in_redir = 1;
            in_file = arguments[i + 1];
            i++; // skip the name of the file
            break;
        }
        else if (strcmp(arguments[i], "&") == 0) {
            continue;
        }
        else {
            child_args[j++] = arguments[i];
        }
    }

    child_args[j] = NULL;

    // If no arguments, return
    if (size_of_arg == 0) {
        return;
    }

    // If the user has requested a built-in command
    if (builtin_cmd(arguments) == -1) {

        if (access(arguments[0], X_OK) != 0) {
            printf("%s: Command not found\n", child_args[0]);
            return;
        }

        // Block signals before forking
        sigset_t mask, prev_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTSTP);
        sigprocmask(SIG_BLOCK, &mask, &prev_mask);

        // Fork a child process
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork invalid");
        } else if (pid == 0) {
            // Child process
             if (in_redir == 1) {
                int fd_in = open(in_file, O_RDONLY, 0);
                if (fd_in < 0) {
                    fprintf(stderr, "Could not open %s for input\n", in_file);
                    exit(1);
                } else {
                    dup2(fd_in, 0);
                    close(fd_in);
                }
            }
            if (out_redir == 1) {
                int fd_out = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    fprintf(stderr, "Could not open %s for output\n", out_file);
                    exit(1);
                } else {
                    dup2(fd_out, 1);
                    close(fd_out);
                }
            }

            // Set child in its own process group
            setpgid(0, 0);

            // Unblock signals in child after setting process group
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);

            // Execute the command
            if (execve(child_args[0], child_args, environ) < 0) {
                printf("%s: Command not found\n", child_args[0]);
                // exit(1);
            }
            exit(0);
        } else {
            // Parent process

            // Unblock signals in parent
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);

            // Add job to job list
            if (strcmp(arguments[size_of_arg - 1], "&") == 0) {
                // If in the background
                addjob(jobs, pid, BG, cmdline);

                struct job_t *job = getjobpid(jobs, pid); // Get newly added job
                printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
            } else {
                // If in the foreground
                addjob(jobs, pid, FG, cmdline);
                waitfg(pid);
                // deletejob(jobs, pid);
            }
        }
    }
}



/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return number of arguments parsed.
 */
int parseline(const char *cmdline, char **argv) {
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to space or quote delimiters */
    int argc;                   /* number of args */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    } else {
        delim = strchr(buf, ' ');
    }

    while (delim) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        } else {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;
    return argc;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) {
    // TODO: DO error handling

    int size_of_arg = 0;
    for (int i = 0; argv[i] != NULL; i++) {
        size_of_arg++;
    }
    if (size_of_arg == 1 || size_of_arg == 2) {
        if (strcmp(argv[0], "quit") == 0 && size_of_arg == 1) {
            exit(0);
        } else if (strcmp(argv[0], "jobs") == 0 && size_of_arg == 1) {
            listjobs(jobs);
            return 0;
        } else if (strcmp(argv[0], "bg") == 0) {
            do_bgfg(argv);
            return 0;
        } else if (strcmp(argv[0], "fg") == 0) {
            do_bgfg(argv);
            return 0;
        }
        return -1;
    }
    return -1;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
    struct job_t *job;
    char *id = argv[1];

    // Check if an ID argument is provided
    if (id == NULL) {
        printf("%s command requires PID or %%jid argument\n", argv[0]);
        return;
    }

    int is_pid;
    int id_val;

    if (id[0] == '%') {  // id is a JID
        is_pid = 0;
        id_val = atoi(id + 1);
        for (int i = 1; id[i] != '\0'; i++){
            if (!isdigit(id[i])) {
                printf("%s: argument must be a PID or %%jid\n", argv[0]); // If a non-digit character is found, return
                return; // Return if a non-digit character is found
            }
        }
    } else {  // id is a PID
        is_pid = 1;
        id_val = atoi(id);
        for (int i = 0; id[i] != '\0'; i++){
            if (!isdigit(id[i])) {
                printf("%s: argument must be a PID or %%jid\n", argv[0]); // If a non-digit character is found, return
                return; // Return if a non-digit character is found
            }
        }
    }


    // Find the job by either PID or JID
    if (is_pid) {
        job = getjobpid(jobs, id_val);
        if (job == NULL) {
            printf("(%s): No such process\n", id);
            return;
        }
    } else {
        job = getjobjid(jobs, id_val);
        if (job == NULL) {
            printf("%s: No such job\n", id);
            return;
        }
    }

    // Send SIGCONT signal to the job's process group
    if (kill(-(job->pid), SIGCONT) < 0) {
        unix_error("kill error");
        return;
    }

    // Update job state based on the command (fg or bg)
    if (strcmp(argv[0], "fg") == 0) {
        job->state = FG;
        waitfg(job->pid); // Wait for the job to finish if it's in the foreground
    } else {
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
    sigset_t mask, prev_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask, &prev_mask); // Block SIGCHLD to avoid race conditions
    while (pid == fgpid(jobs)) {
        // Wait for a signal to arrive
        sigsuspend(&prev_mask);
        // Re-test whether the foreground process is still in the foreground
        pid_t fg_pid = fgpid(jobs);
        if (pid != fg_pid) {
            // If the foreground process changed, break out of the loop
            sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Restore previous signal mask
            break;
        }
        sigprocmask(SIG_SETMASK, &prev_mask, NULL); // Restore previous signal mask
    }
    return;
}



/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) {
    int olderrno = errno;
    pid_t pid;
    int status;

    // Reap all available zombie children
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        struct job_t *job = getjobpid(jobs, pid);

        if (job == NULL) {
            continue; // This child process is not a job managed by the shell
        }

        // Update job state
        if (WIFEXITED(status)) {
            // kill(-pid, SIGCHLD);
            deletejob(jobs, pid); // Child terminated normally, so delete the job
        } else if (WIFSIGNALED(status)) {
            printf("Job [%d] (%d) terminated by signal %d\n", job->jid, pid, WTERMSIG(status));
            deletejob(jobs, pid); // Child terminated by a signal, so delete the job
        } else if (WIFSTOPPED(status)) {
            job->state = ST; // Child stopped, update job state to stopped
            // printf("Job [%d] (%d) stopped by signal %d\n", job->jid, pid, WSTOPSIG(status));
        }
    }

    errno = olderrno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) {
    pid_t pid = fgpid(jobs);

    if (pid > 0) {
        struct job_t *job = getjobpid(jobs, pid); // Get the job using the PID
        if (job != NULL) {
            kill(-pid, SIGINT);
            printf("Job [%d] (%d) terminated by signal %d\n", job->jid, pid, SIGINT);
            deletejob(jobs, pid);
        }
    } else {
        // No foreground process
        printf("\n%s", prompt);
        fflush(stdout);
    }
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) {
    pid_t pid = fgpid(jobs); // Get the PID of the foreground job

    if (pid != 0) {
        struct job_t *job = getjobpid(jobs, pid); // Get the job using the PID
        if (job != NULL) {
            job->state = ST; // Update job state to stopped
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, pid, SIGTSTP);
            kill(-pid, SIGTSTP); // Send SIGTSTP signal to the process group containing the foreground job
        }
    }
}

/*
 * sigusr1_handler - child is ready
 */
void sigusr1_handler(int sig) {
    ready = 1;
}


/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* freejid - Returns smallest free job ID */
int freejid(struct job_t *jobs) {
    int i;
    int taken[MAXJOBS + 1] = {0};
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid != 0)
            taken[jobs[i].jid] = 1;
    for (i = 1; i <= MAXJOBS; i++)
        if (!taken[i])
            return i;
    return 0;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
    int i;

    if (pid < 1)
        return 0;
    int free = freejid(jobs);
    if (!free) {
        printf("Tried to create too many jobs\n");
        return 0;
    }
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = free;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose) {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    return 0; /*suppress compiler warning*/
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) {
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
    int i;
    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG:
                    printf("Running ");
                    break;
                case FG:
                    printf("Foreground ");
                    break;
                case ST:
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ",
                           i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message and terminate
 */
void usage(void) {
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}


