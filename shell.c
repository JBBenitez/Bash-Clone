

#include "shell.h"
#include "parse.h"

/* Constants */
#define DEBUG 0
/* global variable meant to save the process status of a process
 */
static int processStatus=0;
/* global variable meant to save the exit code of the most recent normally terminated process
 */
static int exitCode=0;
typedef struct job_t job; 
/* this is a global foreground job from the job_t struct
 */
static struct job_t *fgJob;
/*job list is the global list for background jobs
 */
static struct job_t *jobList;
/*
 * static const char *shell_path[] = { "./", "/usr/bin/", NULL };
 * static const char *built_ins[] = { "quit", "help", "kill", 
 * "fg", "bg", "jobs", NULL};
*/

/* The entry of your shell program */
void exitCodeReplace(char *argv[], char *cmd);
void jobRemoval(int pid);
int jobInsert(struct job_t *job);
struct job_t* jobLocate(int pid);
struct job_t* jobIDLocate(int jobID);
void printBGJobList();
int jobCount();
/* this is a struct that saves the info for a job
 */
struct job_t {
/*this saves the job id of a job
 */
int jobID;
/*this saves the pid of a job
 */
int pid;
/* this saves the execution state of a job it will either be Running or Stopped
 */
char state[9];
/*this saves the original command that started the job
 */
char *cmd;
/*this saves the aux of a job so i know when to use file redirection or if the job is background or foreground
 */
Cmd_aux aux;
/* this struct is also a linked list so i can connect background jobs to each other
 */
struct job_t *next;
};
/*this handler monitors the status of a child if it has terminated, been stopped or continued
 */
void sigchildHandler(int sig)
{
  struct job_t *currentJob=jobList;
  pid_t pid=0;
  while((pid = waitpid(-1,&processStatus,WNOHANG|WUNTRACED|WCONTINUED)) > 0)
  { 
    /* if the process exited normally itll check if the terminated job has the same pid as the fgJob and itll also check if the fgJob has been set to background or not
     */
    if(WIFEXITED(processStatus))
    {
     if(fgJob!=NULL&&fgJob->pid==pid&&fgJob->aux.is_bg==0)
     {
       currentJob=jobLocate(pid);
       log_job_state(pid, LOG_FG, fgJob->cmd, LOG_TERM);
       exitCode=WEXITSTATUS(processStatus);
       /*if in case the foregroundJob was in the list of background jobs then this line will remove the job from the job list
        */
       if(currentJob!=NULL)
         jobRemoval(pid);
     }
     else
     {
        /* this is for background job termination
         */
        currentJob=jobLocate(pid);
        log_job_state(pid, LOG_BG, currentJob->cmd, LOG_TERM);
        jobRemoval(currentJob->pid);
        exitCode=WEXITSTATUS(processStatus); 
     }
    }
    /* if the process was signalled to terminate whether by signal 2 or 9 then this will become true
    */
    if(WIFSIGNALED(processStatus))
    {
     currentJob=jobLocate(pid);
     /*this is incase the fgJob has not been added to jobList and the fgJob is not null
      */
     if(currentJob==NULL&&fgJob!=NULL)
     { 
       if(fgJob->pid==pid)
        log_job_state(pid, LOG_FG, fgJob->cmd, LOG_TERM_SIG);
     }
     /*this checks in case the job was found in the global job list then it checks it whether the job was foreground or background
      * then the job will be removed from the list.
      */
     else
     {
       if(currentJob->aux.is_bg==0)
       {
         log_job_state(pid, LOG_FG, currentJob->cmd, LOG_TERM_SIG);
         jobRemoval(pid);
       }
       else
       {
         log_job_state(pid, LOG_BG, currentJob->cmd, LOG_TERM_SIG);
         jobRemoval(pid);
       } 
     }
    }
    /* this checks if a signal to stop the process has been sent
     */
    if(WIFSTOPPED(processStatus))
    {
      /*this is to check whether what has stopped is foreground or background
       * if it is foreground then the the aux struct within job will have its is_bg value changed to 1
       * if not it just print that the background job has stopped
       * also the execution state of the job will be changed to stopped no matter what
       */
      currentJob=jobLocate(pid);
      if(fgJob!=NULL&&fgJob->pid==pid&&fgJob->aux.is_bg==0)
      {
        log_job_state(pid, LOG_FG, fgJob->cmd, LOG_STOP); 
        fgJob->aux.is_bg=1;
        snprintf(fgJob->state, 8, "%s", "Stopped");
        jobInsert(fgJob);
      }
      else
      {
        log_job_state(pid, LOG_BG, currentJob->cmd, LOG_STOP);
        snprintf(currentJob->state, 8, "%s", "Stopped");
      }
    }
    /* this is to check if a job has been signalled to continue
     * if the process is background the state will be sent to running 
     * if it is foreground then i will wait for the process to terminate with waitpid
     * jobRemoval will activate here because i never remove a job from the background if it was originally foreground then switched back to background
     * because no one can even see the list of background jobs when a foreground job is running so i did not see the point in removing it
     */
    if(WIFCONTINUED(processStatus))
    {
      currentJob=jobLocate(pid);
      if(currentJob->aux.is_bg==1)
      {
        snprintf(currentJob->state, 8, "%s", "Running");
        log_job_state(currentJob->pid, LOG_BG, currentJob->cmd, LOG_CONT);
      }
      else if(currentJob->aux.is_bg==0)
      {
        snprintf(fgJob->state, 8, "%s", "Running");
        
        log_job_state(fgJob->pid, LOG_FG, fgJob->cmd, LOG_CONT);
        waitpid(fgJob->pid,&processStatus, 0);
        if(WIFEXITED(processStatus))
        {
          exitCode=WEXITSTATUS(processStatus);
          log_job_state(fgJob->pid, LOG_FG, fgJob->cmd, LOG_TERM);     
          jobRemoval(currentJob->pid);
        }
      }
    }
  }
}
/* this is just the basic sigint handler
 * if there is a foreground job then kill is used to send a sigint to fgjob
 * if not only the log_ctrl_c is printed so the shell will not be affected by a ctrl c command
 */
void sigintHandler(int sig)
{
  if(fgJob!=NULL&&fgJob->pid>0)
  {
    log_ctrl_c();
    kill(fgJob->pid,SIGINT);
  }
  else
    log_ctrl_c();
}
/* this is pretty much exatly the same as the sigint handler
 */
void sigtstpHandler(int sig)
{
  if(fgJob!=NULL&&fgJob->pid>0)
  {
    log_ctrl_z();
    kill(fgJob->pid,SIGTSTP);
  }
  else
    log_ctrl_z();
}
int main() {
  char cmdline[MAXLINE];        /* Command line */
  char *cmd = NULL;
  /* masks used to block signals
   */
  sigset_t mask, prev_mask;
  sigemptyset(&mask);
  /* this mask will only block sigchld signals
   */
  sigaddset(&mask, 17);
  /* the rest of this just sets up the handlers for sigchld, sigint and sigtstp
   */
  struct sigaction sigchildAct;
  struct sigaction sigintAct;
  struct sigaction sigtstpAct;
  memset(&sigchildAct, 0, sizeof(struct sigaction));
  sigchildAct.sa_handler = sigchildHandler;
  sigaction(SIGCHLD, &sigchildAct, NULL);
  memset(&sigintAct, 0, sizeof(struct sigaction));
  sigintAct.sa_handler = sigintHandler;
  sigaction(SIGINT, &sigintAct, NULL);
  memset(&sigtstpAct, 0, sizeof(struct sigaction));
  sigtstpAct.sa_handler = sigtstpHandler;
  sigaction(SIGTSTP, &sigtstpAct, NULL);
  /* Intial Prompt and Welcome */
  log_prompt();
  log_help();


  /* Shell looping here to accept user command and execute */
  while (1) {
    char *argv[MAXARGS];        /* Argument list */
    Cmd_aux aux;                /* Auxilliary cmd info: check parse.h */

    /* Print prompt */
    log_prompt();

    /* Read a line */
    // note: fgets will keep the ending '\n'
    if (fgets(cmdline, MAXLINE, stdin) == NULL) {
      if (errno == EINTR)
        continue;
      exit(-1);
    }

    if (feof(stdin)) {  /* ctrl-d will exit shell */
      exit(0);
    }

    /* Parse command line */
    if (strlen(cmdline)==1)   /* empty cmd line will be ignored */
      continue;     

    cmdline[strlen(cmdline) - 1] = '\0';        /* remove trailing '\n' */

    cmd = malloc(strlen(cmdline) + 1);
    snprintf(cmd, strlen(cmdline) + 1, "%s", cmdline);

    /* Bail if command is only whitespace */
    if(!is_whitespace(cmd)) {
      initialize_argv(argv);    /* initialize arg lists and aux */
      initialize_aux(&aux);
      parse(cmd, argv, &aux); /* call provided parse() */

      if (DEBUG)  /* display parse result, redefine DEBUG to turn it off */
        debug_print_parse(cmd, argv, &aux, "main (after parse)");

      /* After parsing: your code to continue from here */
      /*================================================*/
    /*this sets up a currentJob to be used for checking through the global list i use this mainly for the build in commands for SHELL
     */
    struct job_t *currentJob= (struct job_t*) malloc(sizeof(struct job_t));
    /* this is a quick check, should a built in command be used this variable will be set to a 1
     */
    int builtInCmds=0;
    int fdInput=-1,fdOutput=-1;
    pid_t pid=0;
    char path1[100]= "./";
    char path2[100]= "/usr/bin/";
    strcat(path1,argv[0]);
    strcat(path2,argv[0]);
    /*this method gets the exit code ready
     */
    exitCodeReplace(argv,cmd);
    /* this whole set of if/else statements checks if the user wishes to use any of the built in commands
     * so again just to reiterate anytime a built in commnand is used then built in cmds will be set to 1
     */
    if(strcmp(argv[0],"help")==0)
    {
      log_help();
      builtInCmds=1;
    }
    /* so at first i wanted to have a global variable of numOfJobs and decrement everytime i removed a job from the global job list
     * but the actual number of jobs is never checked unless the user uses the jobs command where the number of jobs will be displayed
     * so instead i made a function that counts the number of jobs then log_job_number uses the number of jobs then i made a function
     * that prints all the jobs in the background job list
     */
    else if(strcmp(argv[0],"jobs")==0)
    {
      int numOfJobs=0;
      builtInCmds=1;
      numOfJobs=jobCount();
      log_job_number(numOfJobs);
      printBGJobList();
    }
    /* this is for the kill command, it takes the first arg and that will be the signal used for kill and the 2nd arg will be converted to a pid
     * if the job to be used kill on does not exist the log_kill still prints but nothing really happens
     */
    else if(strcmp(argv[0], "kill")==0)
    {
      builtInCmds=1;
      int signalNum = atoi(argv[1]);
      int pid = atoi(argv[2]);
      currentJob=jobLocate(pid);
      log_kill(signalNum,pid);
      if(signalNum==2&&currentJob!=NULL)
      {
        kill(pid,SIGINT);
        log_job_state(pid, LOG_BG, currentJob->cmd, LOG_TERM_SIG);       
        jobRemoval(pid);
      }
      else if(signalNum==9&&currentJob!=NULL)
        kill(pid,signalNum); 
      else if(signalNum==20&&currentJob!=NULL)
        kill(pid,signalNum);
      else if(signalNum==18&&currentJob!=NULL)
        kill(pid,signalNum);
    }
    /* this is for the fg command if the current job is not in the list then log_jobid_error is printed
     * if the job has state of "Stopped" then a signal is sent to continue it and its is_bg is changed to 0
     * else fgJob now refers to currentJob and waitpid will wait for the for the fgJob to end
     */
    else if(strcmp(argv[0], "fg")==0)
    {
      builtInCmds=1;
      int jobID = atoi(argv[1]);
      currentJob=jobIDLocate(jobID);
      if(currentJob==NULL)
        log_jobid_error(jobID);
      else if(strcmp(currentJob->state, "Stopped")==0)
      {
        currentJob->aux.is_bg=0;
        log_job_move(currentJob->pid, LOG_FG, currentJob->cmd);
        fgJob=currentJob;
        kill(currentJob->pid,18);
      }
      else
      {
       currentJob->aux.is_bg=0;
       log_job_move(currentJob->pid, LOG_FG, currentJob->cmd);
       fgJob=currentJob;
       log_job_state(currentJob->pid,LOG_FG, currentJob->cmd, LOG_CONT);
       waitpid(currentJob->pid,&processStatus,0);
       if(WIFEXITED(processStatus))
       {
        exitCode=WEXITSTATUS(processStatus);
        log_job_state(currentJob->pid, LOG_FG, currentJob->cmd, LOG_TERM);
       }
      }
        
    }
    /* this is to continue running a stopped background job
     * if the job with the jobId is not found then log_jobid_error is printed
     * else the job is then continued with the kill command
     */
    else if(strcmp(argv[0], "bg")==0)
    {
      int jobID = atoi(argv[1]);
      builtInCmds=1;
      currentJob=jobIDLocate(jobID);
      if(currentJob==NULL)
        log_jobid_error(jobID);
      else
      {
        log_job_move(currentJob->pid, LOG_BG, currentJob->cmd);
        kill(currentJob->pid,18);
      }
    }
    /*this quits really nothing else to it
     */
    else if(strcmp(argv[0],"quit")==0)
    {
      log_quit();
      exit(0);
    }
    /* this is where builtInCmds is useful, this will check if the new job is background or foreground then itll check if a built in command has been used 
     */
    if(aux.is_bg==0&&builtInCmds==0)
    {
      /* these next two line set up the mask to block sigchld signals and also to set up a new foreground job.
       * with the jobid, copying the command, setting the state to running and setting up the pid, then fork is activated.
       */
      sigprocmask(SIG_BLOCK, &mask, &prev_mask);
      struct job_t *newfgJob = (struct job_t*) malloc(sizeof(struct job_t));
      newfgJob->jobID=0;  
      newfgJob->cmd = malloc(strlen(cmdline) + 1);
      snprintf(newfgJob->cmd, strlen(cmdline) + 1, "%s", cmdline);
      newfgJob->aux=aux;
      snprintf(newfgJob->state, 8, "%s", "Running");
      pid=newfgJob->pid=fork(); 
      if(pid==0)
      {
        /* this whole section is only for child processes, also the child process is setup with a new group process
         */
        setpgid(0,0);
        /* this unblocks signals in the child process
         */
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        pid=getpid();
        log_start(pid,LOG_FG,cmd);
        /* this is if there is no file redirection
         * the rest of the statements are in case there is no input file, no output, whether it overwrites or whether it appends
         */
        if(aux.in_file==NULL&&aux.out_file==NULL)
        {
          if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
          {
            log_command_error(cmd);
            exit(1);
          }
        }
        else if(aux.in_file!=NULL&&aux.is_append==-1)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          {
            dup2(fdInput,0);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }
          }
          close(fdInput);
        }
        else if(aux.in_file==NULL&&aux.is_append==0)
        {
          fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_TRUNC,0600);
          dup2(fdOutput,1);
          if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
          {
            log_command_error(cmd);
            exit(1);
          }
          close(fdOutput);
        }
        else if(aux.in_file!=NULL&&aux.is_append==0)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          { 
            fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_TRUNC,0600);
            dup2(fdInput,0);
            dup2(fdOutput,1);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }

          }
          close(fdInput);
          close(fdOutput);
        }
        else if(aux.in_file!=NULL&&aux.is_append==1)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          {
            fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_APPEND,0600);
            dup2(fdInput,0);
            dup2(fdOutput,1);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }     
          }
          close(fdInput);
          close(fdOutput);
        }
       else if(aux.in_file==NULL&&aux.is_append==1)
       {
         fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_APPEND,0600);
         dup2(fdOutput,1);
         if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
         {
           log_command_error(cmd);
           exit(1);
         }
         close(fdOutput);
       }
      }
      /* this sets up the global fgJob to refer to newFgJob, then waitpid waits for the fgjob to finish of course if no ctrl c or ctrl z happens 
       * which stop or terminate the process once wait pid ends then the previous mask will be applied
       * the reason it unblocks after waitpid is in case of a situation where background processes end before a foreground process, the logging for the end of background processes
       * will occur once the foreground process either terminates, stops or is terminated by a signal
       */
      fgJob=newfgJob;
      pid=waitpid(fgJob->pid,&processStatus,0);
      sigprocmask(SIG_SETMASK,&prev_mask,NULL);
      if(WIFEXITED(processStatus))
      { 
        if(pid!=-1)
        {
          log_job_state(pid,LOG_FG,fgJob->cmd,LOG_TERM);
          exitCode=WEXITSTATUS(processStatus);
        }
      }
    }
    /* this is basically the same as fg job only with background jobs
     */
    else if(aux.is_bg==1&&builtInCmds==0)
    {
      sigprocmask(SIG_BLOCK, &mask, &prev_mask);
      pid=fork();
      if(pid==0)
      {
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        pid=getpid();
        setpgid(0,0);
        log_start(pid,LOG_BG,cmd);
        if(aux.in_file==NULL&&aux.out_file==NULL)
        {
          if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
          {
 	    log_command_error(cmd);
            exit(1);
          }
        }
        else if(aux.in_file!=NULL&&aux.is_append==-1)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          {
            dup2(fdInput,0);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }
          }
         close(fdInput);
        }
        else if(aux.in_file==NULL&&aux.is_append==0)
        {
         fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_TRUNC,0600);
         dup2(fdOutput,1);
         if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
         {
           log_command_error(cmd);
           exit(1);
         }
         close(fdOutput);
        }
        else if(aux.in_file!=NULL&&aux.is_append==0)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          {
            fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_TRUNC,0600);
            dup2(fdInput,0);
            dup2(fdOutput,1);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }
          }
          close(fdInput);
          close(fdOutput);
        }
        else if(aux.in_file!=NULL&&aux.is_append==1)
        {
          if((fdInput = open(aux.in_file, O_RDONLY)) < 0)
          {
            log_file_open_error(aux.in_file);
            exit(1);
          }
          else
          {
            fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_APPEND,0600);
            dup2(fdInput,0);
            dup2(fdOutput,1);
            if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
            {
              log_command_error(cmd);
              exit(1);
            }
          }
          close(fdInput);
          close(fdOutput); 
        }
       else if(aux.in_file==NULL&&aux.is_append==1)
       {
         fdOutput = open(aux.out_file, O_RDWR|O_CREAT|O_APPEND,0600);
         dup2(fdOutput,1);
         if(execv(path1,argv)==-1&&execv(path2,argv)==-1)
         {
           log_command_error(cmd);
           exit(1);
         }
        close(fdOutput);
       }
      }
      else
      { 
        /* the big difference is in this block of code where bgJob is sent to a function called jobInsert
         */    
	struct job_t *bgJob = (struct job_t*) malloc(sizeof(struct job_t));
        bgJob->pid=pid;
        bgJob->jobID=0;
        bgJob->cmd = malloc(strlen(cmdline) + 1);
        snprintf(bgJob->cmd, strlen(cmdline) + 1, "%s", cmdline);
        bgJob->aux=aux;
        snprintf(bgJob->state, 8, "%s", "Running");
        jobInsert(bgJob);  
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);     
      }
    
    }
    }
    free_options(&cmd, argv, &aux);
  }
  return 0;
}
/* this function replaces the string "$?" with the exit code of the most recently terminated process
 */
void exitCodeReplace(char *argv[],char *cmd)
{
  int i=0;
  char replacementCode[4];
  sprintf(replacementCode,"%d",exitCode);
  /*for loop checks all the arguements and if the "$?" is found its replaced with the exit code
   */
  for(i=0;argv[i];i++)
  {
   if(strcmp(argv[i],"$?")==0)
   {
     log_replace(i,replacementCode);
     sprintf(argv[i],"%d",exitCode);
   }
  }
  return;
}
/* this job insert function inserts the newest background job into the global job list
 */
int jobInsert(struct job_t *job)
{
  int jobID=1;
  struct job_t *currentJob = jobList;
  /* if the jobList is null then a job is added no matter what with jobid of 1
   */
  if(jobList==NULL)
  {
    jobList=job;
    jobList->jobID=jobID;
    return 1;
  }
  else
  {
    while(currentJob!=NULL)
    { 
      /*so because i dont remove the job from the list if it becomes a foreground job theres a chance that duplicates may be added if i were to create a background job
       * bring it to the foreground and put it back in the background. this line stops that if since PID`s are unique then if the job going through the list has the exact same pid as the job to be added then
       * no new job will be added to the list
       * else a new job will be added and the new job will have the job id of the last job in the list incremented by one
       */
      if(currentJob->pid==job->pid)
        return 0;
      if(currentJob->next==NULL)
      {
        jobID=currentJob->jobID+1;
        job->jobID=jobID;
        currentJob->next=job;
        return 1;
      }
      currentJob=currentJob->next;
    }
  }
  
return 0;
}
/*this just goes through the list printing out all jobs in the global job list
 */
void printBGJobList()
{
  struct job_t *currentJob = jobList;
  while(currentJob!=NULL)
  {
    log_job_details(currentJob->jobID,currentJob->pid,currentJob->state,currentJob->cmd);
    currentJob=currentJob->next;
  }
  return;
}
/*this function removes a job in the list with the pid associated it with it
 */
void jobRemoval(int pid)
{
  struct job_t *currentJob = jobList;
  /*if the job to be removed is the only one in the list then just make the global list null
   *else if the first job is to be removed and there is more than one in the list then just make the global list`s first job be the next in the list
   */
  if(jobList->next==NULL)
  {
    jobList=NULL;
    return;
  }
  else if(jobList->pid==pid&&jobList->next!=NULL)
  {
    jobList=jobList->next;
    return;
  }
  else
  { 
    /* this is just a loop to find the correct job to remove
     * if the currentJobs pointer to the next job matches the correct pid then the currentJob`s next will point to the next jobs next, sorry if that was a bad way to explain it
     */
    while(currentJob!=NULL)
    { 
      if(currentJob->next->pid==pid)
      {
        currentJob->next=currentJob->next->next;
        break;
      }
      currentJob=currentJob->next;
    }
  }
  return;
}
/*this locates a job based on the pid and returns the job if it found the one with the correct pid associated with it
 */
struct job_t* jobLocate(int pid)
{
  struct job_t *currentJob = jobList;  
  while(currentJob!=NULL)
  {
    if(currentJob->pid==pid)
      return currentJob;
    currentJob=currentJob->next;
  }
  return NULL;
}
/* this is basically the same as the other joblocate only this locates based on the job id instead of the pid
 */
struct job_t* jobIDLocate(int jobID)
{
  struct job_t *currentJob = jobList;
  while(currentJob!=NULL)
  {
    if(currentJob->jobID==jobID)
      return currentJob;
    currentJob=currentJob->next;
  }
 return NULL;
}
/* this just counts the amount of jobs in the global job list
 */
int jobCount()
{
  int numOfJobs=0;
  struct job_t *currentJob=jobList;
  while(currentJob!=NULL)
  {
   numOfJobs++;
   currentJob=currentJob->next;
  }
  return numOfJobs;
}


