#include "helpers.h"
// Your helper functions need to be here.


//  === BACKGROUND PROCESS === //

void sigchld_handler(int sig) {
    int olderrno = errno;
    sigchld_flag = 1; // check on next prompt
    errno = olderrno;
}

void reap_background(list_t* bg_list, int* exit_status, int option) {
    pid_t pid;
    bgentry_t* bgentry = NULL;
    while ((pid=waitpid(-1, exit_status, option)) > 0) {
        bgentry = remove_bglist_pid(bg_list, pid);
        if (bgentry != NULL) { // indeed background process
            fprintf(stdout, BG_TERM, bgentry->pid, bgentry->job->line);
            free_bgentry(bgentry);
        }
    } // if pid = -1, no childs to reap. ok since if all backgrounds are reaped no more children.
     bg_list_length = bg_list->length;
}

bgentry_t* remove_bglist_pid(list_t* bg_list, pid_t pid) {
    int index = 0;
    node_t* node = bg_list->head;
    while(node!= NULL && ((bgentry_t*) (node->data))->pid != pid) {
        index++;
        node = node->next;
    }
    if (node == NULL) { // not found
        return NULL;
    }
    return (bgentry_t*) (RemoveByIndex(bg_list, index));
}

// === SIGU2 HANDLER STUFF ==== //

void reverse(char* s) {
    int front, back, length;
    length = strlen(s);
    back = length - 1;
    char temp;
    for (front = 0; front < length /2; front++, back--) {
        temp = s[back];
        s[back] = s[front];
        s[front] = temp;
    }
    
}

char* itoa(int num) { // ignore negative since will never be a parameter
    static char buf[33];
    int pos = 0;
    if (num == 0) {
        buf[pos++] = '0';
        buf[pos] = '\0';
        return buf;
    }
    int i;
    for (i=num%10; num != 0; num=num/10, i=num%10) {
        buf[pos++] = (char) (i + 48);
    }
    buf[pos] = '\0';
    
    reverse(buf);
    return buf;
}

void sigusr2_handler(int sig) { // sigchild handler doesn't matter since only reap is controlled/set.
    int olderrno = errno;
    char msg[] = "Num of Background processes: ";
    write(STDERR_FILENO, msg, sizeof(msg)); // during redirection keep in mind
    char* length = itoa(bg_list_length);
    write(STDERR_FILENO, length, sizeof(length));
    write(STDERR_FILENO, "\n", 1);
    
    errno = olderrno;
}

//  === COMMANDS === //

void change_directory(job_info* job_command) {
    char* dir;
    
    dir = (job_command->procs->argc == 1) ? getenv("HOME") : (job_command->procs->argv)[1];
    if (chdir(dir) == -1)
        fprintf(stderr, "%s", DIR_ERR);
    else {
        char* absolute_path = getcwd(NULL, 0);
        fprintf(stdout, "%s\n", absolute_path);
        free(absolute_path);
    }
}

void estatus(int exit_status) {
    fprintf(stdout, "%d\n", exit_status);
}


void bglist(list_t* bg_list) {
    PrintLinkedList(bg_list);
}

void history(list_t* history_list) {
    node_t* curr = history_list->head;
    int i;
    for (i = 0; i < history_list->length && curr!= NULL; i++, curr=curr->next) {
        fprintf(stdout, "%d: %s\n", i+1, (char*)(curr->data));
    }
}


int reexecute(list_t* history_list, job_info** job_command, char** cmd) {
    node_t* curr = history_list->head;
    long pos = 0; // default the first one

    // find index of history list.
    char* temp = malloc(strlen(*cmd) * sizeof(char) + 1);
    strcpy(temp, *cmd);
    char *token = strtok(temp, " ");
    token++;
    if (*token != '\0') { // 
        char *end;
        pos = strtol(token, &end, 10);
        //if (end == token) dont conside rsince strol returns 0 on failure either way (default case).
            //return 2;
    } 
    free(temp);

    // iterating to correct position
    if (pos > 0)
        pos--;
    int i;
    for (i = 0; i < pos && curr != NULL; i++, curr = curr->next);
    if (curr == NULL && (i != pos -1)) // invalid size, or not in history yet.
        return 1; // change

    // reassigning values for job and cmd.
    char* hist = (char*) (curr->data);
    char* new_command = malloc(strlen(hist)*sizeof(char) + 1);
    strcpy(new_command, hist);
    free_job(*job_command);
    free(*cmd);
    *cmd = new_command;
    *job_command = validate_input(new_command);
    return 0;
}

void end(list_t* bg_list) {
    node_t* curr = bg_list->head;
    while (curr != NULL) {
        kill(((bgentry_t*) (curr->data))->pid, SIGKILL);
        curr = curr->next;
    }
}

// bgentry

bgentry_t* make_bgentry(job_info* job, pid_t child_pid, time_t seconds) {
    //job_info* job = validate_input(cmd); // creating a new job for the process to reference
    bgentry_t* bgentry = malloc(sizeof(bgentry_t));
    bgentry->job = job;//job;
    bgentry->pid = child_pid; //child process.
    bgentry->seconds = seconds;
    return bgentry;
}
void free_bgentry(bgentry_t* bgentry) {
    free_job(bgentry->job);
    bgentry->job = NULL;
    free(bgentry);
}

int bgentry_comparator(const void* bg1, const void* bg2) {
    bgentry_t* bgentry1 = (bgentry_t*) bg1;
    bgentry_t* bgentry2 = (bgentry_t*) bg2;

    if (bgentry1->seconds < bgentry2->seconds)
        return -1;
    else if (bgentry1->seconds > bgentry2->seconds)
        return 1;
    else
        return 0;
}

void bgentry_printer(void* data) { // ** CHANGES TO linkedlist. printer used to be (void* data, void* fp). But now only takes one arg.
    bgentry_t* bgentry = (bgentry_t*) data;
    print_bgentry(bgentry);
}

void bgentry_deleter(void* data) { // called when background (child) dies so should free job_info.
    bgentry_t* bgentry = (bgentry_t*) data;
    free_job(bgentry->job);
    free(bgentry);
}

void add_history(list_t* history_list, char* cmd) {
    char* job_command = malloc((strlen(cmd)*sizeof(char)) + 1);
    strcpy(job_command, cmd);
    InsertAtHead(history_list, job_command);
    if (history_list->length > 5) {
        free(RemoveFromTail(history_list));
    }
}


int command_comparator(const void* str1, const void* str2) {
    return strcmp(str1, str2); // always insert at head.
}

void command_printer(void* data) {
    return;
}

void command_deleter(void* data) {
    free((char*)data); // only relevant job
}

// == FILE REDIRECTS == //


int valid_files(job_info* job) {
    char* infile = job->in_file;
    char* outfile = job->out_file;
    char* errfile = job->procs->err_file;
    if (infile != NULL && outfile != NULL && errfile != NULL) {
        return strcmp(infile, outfile) != 0 && strcmp(infile, errfile) != 0 && strcmp(errfile, outfile) != 0;
    } else if (infile != NULL && outfile != NULL) {
        return strcmp(infile, outfile) != 0;
    } else if (infile != NULL && errfile != NULL) {
        return strcmp(infile, errfile) != 0;
    } else if (errfile != NULL && outfile != NULL) {
        return strcmp(errfile, outfile) != 0;
    }
    return 1;
}


void save_std_streams(int* fds) {
    fds[0] = dup(STDIN_FILENO);
    fds[1] = dup(STDOUT_FILENO);
    fds[2] = dup(STDERR_FILENO);
}

void restore_std_streams(int* fds) {
    dup2(fds[0], STDIN_FILENO);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[2], STDERR_FILENO);
    
    close_saves(fds);
}

void close_saves(int* fds) {
    close(fds[0]);
    close(fds[1]);
    close(fds[2]);
}
void load_redirects(job_info* job) {
    if (job->in_file != NULL) {
        // attempt open
        int new_stdin = open(job->in_file, O_RDONLY, 0);
        if (new_stdin == -1) //failed for some reason
            return; // cleanup!!
        dup2(new_stdin, STDIN_FILENO);
        close(new_stdin);
    }
    if (job->out_file != NULL) {
        int new_stdout = open(job->out_file,  O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (new_stdout == -1) //failed for some reason
            return;
        dup2(new_stdout, STDOUT_FILENO);
        close(new_stdout);
    }
    if (job->procs->err_file != NULL) {
        int new_stderr = open(job->procs->err_file,  O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (new_stderr == -1) //failed for some reason
            return;
        dup2(new_stderr, STDERR_FILENO);
        close(new_stderr);
    }
}

void pipe_redirect(job_info* job) {
    int prev_pipe[2];
    int curr_pipe[2];

    proc_info* curr = job->procs;

    while (curr != NULL) {
        if (curr->next_proc != NULL) {
            pipe(curr_pipe); // assume works.
        }
        if(fork() == 0) {
            if(curr != job->procs) {
                dup2(prev_pipe[0], STDIN_FILENO);
            }
            if (curr->next_proc != NULL) {
                dup2(curr_pipe[1], STDOUT_FILENO);
            }
            if (curr != job->procs) {
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }
            if (curr->next_proc != NULL) {
                close(curr_pipe[0]);
                close(curr_pipe[1]);
            }
            execvp(curr->cmd, curr->argv);
            
        }

        if (curr != job->procs) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }
        if (curr->next_proc != NULL) {
            prev_pipe[0] = curr_pipe[0];
            prev_pipe[1] = curr_pipe[1];
        }
        curr = curr->next_proc;
    }
}

