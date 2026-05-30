#include "icssh.h"
#include "helpers.h"
#include "linkedlist.h"
#include <readline/readline.h>

void cleanup(job_info* job, char* curline)
{
	free_job(job);  // Free job struct and associated memory (allocated by validate_input)
	free(curline);  // Free user-entered command string (allocated by readline)

	// validate_input(NULL) frees internal dynamically allocated memory reused call after call to process the user-entered job command repeatedly
	// This call ensures valgrind is happy! 
	validate_input(NULL);  
}
volatile sig_atomic_t sigchld_flag = 0;// added.
volatile int bg_list_length = 0;

int main(int argc, char* argv[]) {
	int exec_result;
	int exit_status = -100;
	pid_t pid;
	pid_t wait_result;

    sigset_t mask_all, mask_child, prev_mask;
    sigfillset(&mask_all);
    sigemptyset(&mask_child);
    sigaddset(&mask_child, SIGCHLD);
    time_t now; // added
    
    list_t *bg_list, *history_list;
    bg_list = CreateList(bgentry_comparator, bgentry_printer, bgentry_deleter);
    history_list = CreateList(command_comparator, command_printer, command_deleter);

	// Refers to memory allocated by the readline(). This space is allocated for each user-entered job (the command-line entry). 
	char* curline = NULL;

#ifdef GS  // DO NOT MODIFY. FOR AUTOGRADER
    rl_outstream = fopen("/dev/null", "w");
#endif

	// Setup segmentation fault handler (provided)
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}
    
    // Setup child termination handler 
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
		perror("Failed to set child handler");
		exit(EXIT_FAILURE);
	}
    // Setup usr2 signal handler
    if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
		perror("Failed to set usr2 handler");
		exit(EXIT_FAILURE);
	}
    // print the prompt & wait for the user to enter commands string
	while ((curline = readline(SHELL_PROMPT)) != NULL && time(&now)) {
		// validate_input() parses the user command string in curline into the job struct format. 
		// Dynamically allocates the job struct and memory for line field (copy of curline) 
		// The job struct and the memory referenced by job.line is deallocated by free_job
        // On error, no dynamic memory is allocated and function prints an error message 
		job_info* job = validate_input(curline);
        if (job == NULL) { // Command was empty string or invalid
			free(curline);
			continue;
		}
        if (sigchld_flag == 1) {
            sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
            reap_background(bg_list, &exit_status, WNOHANG);
            sigchld_flag = 0;
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        }

        if (job->nproc > 1) {
            pipe_redirect(job);
            free_job(job);
            free(curline);
            continue;
        }

        //Prints out the job linked list structure for debugging
        #ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
     		debug_print_job(job);
        #endif
        if (valid_files(job) == 0) {
            free_job(job);
        	free(curline);
            printf(RD_ERR);
            continue;
        }
        int saves[3];
        save_std_streams(saves);

        load_redirects(job);
        //history
        if (strcmp(job->procs->cmd, "history") == 0) {
            history(history_list);
            free_job(job);
        	free(curline);
            restore_std_streams(saves);
            continue;
        }
        if ((job->procs->cmd)[0] == '!') {
            if(reexecute(history_list, &job, &curline) == 1) {
                restore_std_streams(saves);
                free_job(job);
            	free(curline);
                continue;
            }
            else {
                printf("%s\n" , curline);
                load_redirects(job);
            }
        }
        if (strcmp(job->procs->cmd, "exit") == 0) {
            restore_std_streams(saves);
			cleanup(job, curline);
            sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);
            end(bg_list); //bglist
            reap_background(bg_list, &exit_status, 0);
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            //delete lists
            DeleteList(bg_list);
            //DeleteList(history_list);
            return 0;
		}
        add_history(history_list, curline);
        
		// Example built-in: basic exit (modify for assignment)
        // ALL BUILT-IN COMMANDS
        // handling cd:
        if (strcmp(job->procs->cmd, "cd") == 0) {
            change_directory(job);
            restore_std_streams(saves);
            free_job(job);
        	free(curline);
            continue;
		}
        if (strcmp(job->procs->cmd, "estatus") == 0) {
            estatus(exit_status);
            restore_std_streams(saves);
            free_job(job);
        	free(curline);
            continue;
		}
        if (strcmp(job->procs->cmd, "bglist") == 0) {
            bglist(bg_list);
            restore_std_streams(saves);
            free_job(job);
        	free(curline);
            continue;
        }
    
		// example of good error handling!
        // create the child process
        // SIGNALS mask child 
        sigprocmask(SIG_BLOCK, &mask_child, &prev_mask);
		if ((pid = fork()) < 0) {
			cleanup(job, curline);
			exit(EXIT_FAILURE);
		}
		if (pid == 0) {  //If zero, then it's the child process
            //get the first command in the job list to execute
            // SIGNALS unmask and reset with previous mask CHECK
            sigprocmask(SIG_SETMASK, &prev_mask, NULL); // reset masks before entering child process.
            
		    proc_info* proc = job->procs;
			exec_result = execvp(proc->cmd, proc->argv);

            restore_std_streams(saves);
            
			if (exec_result < 0) {  //Error checking
				printf(EXEC_ERR, proc->cmd); 
			    cleanup(job, curline);
				exit(EXIT_FAILURE);
			}
             if (job->bg != 1)
                 free_job(job);
		} else {
            // As the parent, add 
            if (job->bg == 1){// is a background:
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                sigprocmask(SIG_BLOCK, &mask_all, &prev_mask); // resets mask to allow next block no overwrite
                bgentry_t* bgentry = make_bgentry(job, pid, now); // pid 
                InsertInOrder(bg_list, bgentry);
                bg_list_length = bg_list->length;
                sigprocmask(SIG_SETMASK, &prev_mask, NULL); // reset signal for next command
            }
        	// As the parent, wait for the foreground job to finish
            // check if foreground or background and act accordingly.
            else { // inside = OG
                sigprocmask(SIG_SETMASK, &prev_mask, NULL); // allows sigchld handler
    			wait_result = waitpid(pid, &exit_status, 0);
    			if (wait_result < 0) {
    			    cleanup(job, curline);
    				exit(EXIT_FAILURE);
    			}
                free_job(job);  // if a foreground job, we no longer need the data
                
            }
            restore_std_streams(saves);
		}
		free(curline);
	}
    
	validate_input(NULL);
    DeleteList(bg_list);
    DeleteList(history_list);

#ifndef GS // DO NOT MODIFY. FOR AUTOGRADER
	fclose(rl_outstream);
#endif

	return 0;
}
