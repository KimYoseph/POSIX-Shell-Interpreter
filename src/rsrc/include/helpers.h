#ifndef HELPERS_H
#define HELPERS_H

#include "icssh.h"
#include "linkedlist.h"
#include <errno.h>
// A header file for helpers.c
// Declare any additional functions in this file


// sigchild handling

extern volatile sig_atomic_t sigchld_flag;

void sigchld_handler(int sig);

void reap_background(list_t* bg_list, int* exit_status, int option);

bgentry_t* remove_bglist_pid(list_t* bg_list, pid_t pid);


// sigusr2 handling

extern volatile int bg_list_length;

void reverse(char* s);

char* itoa(int num);

void sigusr2_handler(int sig);

// commands


void change_directory(job_info* job_command);

void estatus(int exit_status);

void bglist(list_t* bg_list);

void history(list_t* history_list);

int reexecute(list_t* history_list, job_info** job_command, char** cmd);

void end(list_t* bg_list);


// bgentry list
bgentry_t* make_bgentry(job_info* job, pid_t child_pid, time_t seconds);

void free_bgentry(bgentry_t* bgentry);
    
int bgentry_comparator(const void* bg1, const void* bg2);

void bgentry_printer(void* data);

void bgentry_deleter(void* data);

// list_t for job_info
void add_history(list_t* history_list, char* cmd);

int command_comparator(const void* str1, const void* str2);

void command_printer(void* data);

void command_deleter(void* data);

// file redirections

int valid_files(job_info* job);

void save_std_streams(int* fds);

void restore_std_streams(int* fds);

void close_saves(int* fds);

void load_redirects(job_info* job);

void pipe_redirect(job_info* job);

#endif