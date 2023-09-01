/* Do not modify this file */
#ifndef LOGGING_H
#define LOGGING_H

#define LOG_FG 0
#define LOG_BG 1

#define LOG_TERM 0
#define LOG_TERM_SIG 1
#define LOG_CONT 2
#define LOG_STOP 3

void log_prompt();
void log_replace(int arg, const char* exitcode);
void log_help();
void log_quit();
void log_command_error(const char *line);
void log_start(int pid, int type, const char *cmd);
void log_kill(int signal, int pid);
void log_job_move(int pid, int type, const char *cmd);
void log_jobid_error(int job_id);
void log_ctrl_c();
void log_ctrl_z();
void log_job_state(int pid, int type, const char *cmd, int transition);
void log_file_open_error(const char *file_name);
void log_job_number(int num_jobs);
void log_job_details(int job_id, int pid, const char *state, const char *cmd);

#endif /*LOGGING_H*/
