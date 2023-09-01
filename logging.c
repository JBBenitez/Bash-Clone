/* Do Not Modify This File */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"

#define shell_log(s) printf("\033[1;31m%s%s\n\033[0m",log_head, s); fflush(stdout)

#define shell_write(s) char output[255] = {0}; sprintf(output,"\033[1;31m%s%s\033[0m", log_head, s); write(STDOUT_FILENO, output, strlen(output));

const char *log_head = "[MALOG]";
/* Outputs the Help: All the Built-in Commands */
void log_help() { 
  shell_log("Welcome to MASH (MAson SHell)!");
  shell_log("Built-in Commands: fg, bg, jobs, kill, quit, help.");
  shell_log("\tkill SIGNAL PID");
  shell_log("\tfg JOBID");
  shell_log("\tbg JOBID");
}

/* Outputs the message after running quit */
void log_quit(){
  shell_log("Thanks for using MASH! Good-bye!");
}

/* Outputs the prompt */
void log_prompt() {
  printf("MASH>> ");
  fflush(stdout);
}

/* Outputs a notification of a replacement from $? to an exit code */
void log_replace(int arg, const char* exitcode) {
  char buffer[255] = {0};
  sprintf(buffer, "Argument replacement performed on argument %d using exit code %s", arg, exitcode);
  shell_log(buffer);
}

/* Output when the command is not found
 * eg. User typed in ls instead of /bin/ls and exec returns an error
 */ 
void log_command_error(const char *line) {
  char buffer[255] = {0};
  sprintf(buffer, "Error: %s: Command Cannot Load", line);
  shell_log(buffer);
}

/* Output when starting a foreground or background process */
void log_start(int pid, int type, const char *cmd) {
  char buffer[255] = {0};
  sprintf(buffer, "%s Process %d: %s Started", (type==LOG_FG)?"Foreground":"Background", pid, cmd);
  shell_log(buffer);
} 

/* Output when a process is moved into the foreground or background using fg or bg on the job */
void log_job_move(int pid, int type, const char *cmd) {
  char buffer[255] = {0};
  sprintf(buffer, "Built-in Command %s for Process %d: %s", (type==LOG_FG)?"fg":"bg", pid, cmd);
  shell_log(buffer);
}

/* Output when the given job id is not found */
void log_jobid_error(int job_id) {
  char buffer[255] = {0};
  sprintf(buffer, "Error: Job ID %d Not Found in Background Job List", job_id);
  shell_log(buffer);
}

/* Output when kill command is used */
void log_kill(int signal, int pid) {
  char buffer[255] = {0};
  sprintf(buffer, "Built-in Command kill for Sending Signal %d to Process %d", signal, pid);
  shell_log(buffer);
}

/* Output when ctrl-c is received */
void log_ctrl_c() {
  shell_log("Keyboard Combination control-c Received");
}

/* Output when ctrl-z is received */
void log_ctrl_z() {
  shell_log("Keyboard Combination control-z Received");
}


/* Output when a job changes state.
 * (Signal Handler Safe Outputting)
 */
void log_job_state(int pid, int type, const char *cmd, int transition) {
  char buffer[255] = {0};
  static const char* msgs[] = {"Terminated Normally", "Terminated by Signal", "Continued", "Stopped"};
  if (transition < 0 || transition >= 4) {
	  shell_write("Invalid input to log_job_state\n");
	  return;
  }
  sprintf(buffer,"%s Process %d: %s %s\n",(type==LOG_FG)?"Foreground":"Background", pid, cmd, msgs[transition]);
  shell_write(buffer);
}

/* Output on file open errors */
void log_file_open_error(const char *file_name) {
  char buffer[255] = {0};
  sprintf(buffer, "Error: Cannot Open File %s", file_name);
  shell_log(buffer);
}

/* Output to list the job counts */
void log_job_number(int num_jobs){
  char buffer[255] = {0};
  sprintf(buffer, "%d Job(s)", num_jobs);
  shell_log(buffer);
}

/* Output to detail a single job */
void log_job_details(int job_id, int pid, const char *state, const char *cmd){
  char buffer[255] = {0};
  sprintf(buffer, "Job %d: Process %d: %s %s", job_id, pid, state, cmd);
  shell_log(buffer);
}
