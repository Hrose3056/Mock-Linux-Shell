#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <signal.h>
#include <errno.h>

#define NTASK 32
#define MAXWORD 32
#define MAXLINE 132
#define MAX_NTOKEN MAXWORD
#define TERMINATED 0
#define NOT_TERMINATED 1

using namespace std;

/*
*	Author: Hannah Desmarais
* 	CCID: hdesmara
*	
* 	This is a shell program which allows a user to change directories, print the current directory,
*	view current tasks, run programs as tasks, send signals to those processes, and view their 
*	current status. 
*/

typedef struct task {
	string command;
	int index;
	pid_t pid;
	int terminated;
} task;

/*
* This function will take in a string and split it based on the field delimiters provided. It will
* fill the outToken array with the seperated strings and return the total number of tokens created.
* 
* Arguments:
*	-inStr: the string being parsed
*	-token: the container for the parsed string
*	-fieldDelim: the characters used to split the string
* Returns:
*	0 if no tokens are found or the number of tokens found
*/
int split(string inStr, char token[][MAXWORD], char fieldDelim[]){
	int i, count;
	char *tokenp;
	
	count = 0;
	
	for (i = 0; i < MAX_NTOKEN; i++)
		memset(token[i], 0 , sizeof(token[i]));
	
	string inStrCpy = inStr; //create a copy of the string passed to the function
	if((tokenp = strtok(&inStr[0], fieldDelim)) == NULL){
		return 0; //return 0 if no token is found
	}
	//store first token if found in if statement above
	strcpy(token[count], tokenp);
	count++;
	
	// This loop captures each token in the string and stores them in token. 
	while((tokenp = strtok(NULL, fieldDelim))!= NULL) {
		strcpy(token[count], tokenp);
		count++;
	}
	
	inStr = inStrCpy;
	return count;
}

/*
* This function will set the CPU time limit to the values specified in seconds. The soft limit will
* be 8 minutes and the hard limit will be 10 miniutes. 
*/
void setLimit(){
	struct rlimit lim;
	lim.rlim_cur = 480; //set soft limit to 8 minutes
	lim.rlim_max = 600; //set hard limit to 10 minutes
	
	if (setrlimit(RLIMIT_CPU, &lim) == -1)
		printf("SETRLIMIT FAILED! %s\n", strerror(errno));

}

/*
* The getTime() function will capture the times of the user CPU, system CPU, and the terminated 
* children. It stores it in the tms struct provided by sys/time.h and returns it.
*
* Arguments:
*	-time: the clock_t time being captured
* Returns:
*	returns a struct with the user and system CPU times of the parent and it's children.
*/
struct tms getTime(clock_t &time){
	struct tms timeStruct;
	
	if ((time = times(&timeStruct)) == -1) // Capture times
		cout<< "times error"<< endl;
	
	return timeStruct;
}

/*
* The printTimes function will print the times in seconds of the total time, the msh379 system and
* user times, and the system and user times of the children processes.
* 
* Arguments:
*	-start: the starting time of the program
*	-end: the end time of the program
*	-tmsStart: the struct containing the system and user times of the parent and children at the 
*		       start of the program
*	-tmsEnd: the struct containing the system and user times of the parent and children at the
*			 end of the program
*/
void printTimes(clock_t &start, clock_t &end, struct tms *tmsStart, struct tms *tmsEnd){
	static long clockTick = 0;
	
	cout<< endl;
	
	if (clockTick == 0) //fetch clock ticks per second
		if ((clockTick = sysconf(_SC_CLK_TCK)) < 0) perror("sysconf error");
	
	//divide  by click ticks to get seconds	
	printf("	Total time:               %7.2f (seconds)\n", (end - start)/ (double) clockTick); 
	
	printf("	msh379 user CPU time:     %7.2f (seconds)\n", 
		(tmsEnd->tms_utime - tmsStart->tms_utime) / (double) clockTick);
	
	printf("	msh379 system CPU time:   %7.2f (seconds)\n", 
		(tmsEnd->tms_stime - tmsStart->tms_stime) / (double) clockTick);
		
	printf("	Children user CPU time:   %7.2f (seconds)\n",
		(tmsEnd->tms_cutime - tmsStart->tms_cutime) / (double) clockTick);
	
	printf("	Children system CPU time: %7.2f (seconds)\n",
		(tmsEnd->tms_cstime - tmsStart->tms_cstime) / (double) clockTick);
}

/*
* This function will print the current working directory.
*/
void pdir(){
	char *ptr;

	if (getcwd(ptr, 1024) == NULL)
		printf("GETCWD FAILED! %s\n", strerror(errno));
	else cout<< "	cwd: "<< ptr<< endl;
}

/*
* The cdir function will change the working directory to a pathname specified by the user. If the 
* user enters an environment variable such as HOME or PWD it will extract it and add it to the 
* pathname. Otherwise it will go to the exact pathname specified. This assumes that environment
* variables will have the $ before them and only appear as the first variable of the path.
* 
* Arguments:
* 	-token: the char array containing the parsed tokens of the command entered
*/
void cdir(char token[][MAXWORD]){
	string pathName;
	
	//If the first char of the token is $ then we need to extract the env. variable name
	if (((string)token[1])[0] == '$'){
		char* ptr;
		size_t index;
		//Find first occurance of '/' so we know where the env. variable ends
		if((index = ((string)token[1]).find('/')) == string::npos) 
			index = ((string)token[1]).length();

		if((ptr = getenv(((string)token[1]).substr(1, index-1).c_str())) == NULL){
			printf("GETENV FAILED! %s\n", strerror(errno));
			return;
		}
		
		//add the extracted env variable to the place inside the user wants to go
		pathName = (string)ptr + ((string)token[1]).erase(0,index);
		
		if (chdir(pathName.c_str()) < 0){
			cout<< "	chdir to " << token[1]<< " failed" << endl;
			return;
		}
	}
	else if (chdir(token[1]) < 0) {
		cout<< "	chdir to " << token[1]<< " failed" << endl;
		return;
	}
	else cout<< "	chdir to "<< token[1]<< " successful" << endl;
}

/*
* The lstasks function will print the pid, index, and commands enteres for all accepted tasks
* that have not been terminated by the user. 
*
* Arguments:
*	-tasks: a vector of the struct task containing accepted tasks
*/
void lstasks(vector <task> tasks){
	for (int i = 0; i < tasks.size(); i++){
		if (tasks[i].terminated != TERMINATED)
			printf("	%d:   (pid: %d, command entered: %s)\n", tasks[i].index, tasks[i].pid, 
				&(tasks[i].command)[0]);
	}
}

/*
* This function will attempt to run a command with up to 4 arguments. If successful, it will return 1.
* If unsuccessful it will return 0.
*
* Arguments:
*	-token: the char array containing the parsed tokens of the command entered
*	-tokenNum: the number of tokens in token
*/
void run(char token[][MAXWORD], int tokenNum){
	if (tokenNum == 2){
		if(execlp(token[1] , token[1], (char *) NULL) < 0){
			printf("EXECLP FAILED! %s\n", strerror(errno));
			exit(0);
		}
	}
	if (tokenNum == 3){
		if(execlp(token[1], token[1], token[2], (char *) NULL) < 0){
			printf("EXECLP FAILED! %s\n", strerror(errno));
			exit(0);
		}
	}
	if (tokenNum == 4){
		if(execlp(token[1], token[1], token[2], token[3], (char *) NULL) < 0){
			printf("EXECLP FAILED! %s\n", strerror(errno));
			exit(0);
		}	
	}
	if (tokenNum == 5){
		if(execlp(token[1], token[1], token[2], token[3], token[4], (char *) NULL) < 0){
			printf("EXECLP FAILED! %s\n", strerror(errno));
			exit(0);
		}
	}
	if (tokenNum == 6) {
		if(execlp(token[1], token[1], token[2], token[3], token[4], token[5], (char *) NULL) < 0){
			printf("EXECLP FAILED! %s\n", strerror(errno));
			exit(0);
		}
	}
}

/*
* The addTask function will add a task to the list and create the task struct with the information 
* of that task. 
*
* Arguments:
*	-tasks: a vector of the struct task containing accepted tasks
*	-command: the command entered by the user
*	-pid: the pid of the process being added to tasks
*/
void addTask(vector<task> &tasks, string command, pid_t pid){
	task newTask;
	newTask.terminated = NOT_TERMINATED;
		
	//if it is the first task in the list, set the index to 0
	if(tasks.size() == 0) newTask.index = 0;
	else newTask.index = tasks.size();
	
	newTask.command = command;
	newTask.pid = pid;
	
	tasks.push_back(newTask);
}

/*
* This function will send the SIGSTOP signal to a process indicated by the pid of the taskmin the
* index specified by the user. 
*
* Arguments:
*	-index: the index of the process the user wishes to stop
*	-tasks: a vector of the struct task containing accepted tasks
*/
void stop(int index, vector<task> &tasks){
	if (tasks.empty()) {
		cout<< "	No tasks have been accepted yet!"<< endl;
		return;
	}
	if (index >= tasks.size() || index < 0) {
		cout<< "	Index out of bounds!"<< endl;
		return;
	}
	pid_t pid = tasks[index].pid;
	
	if (kill(pid, SIGSTOP) < 0)
		printf("SIGSTOP FAILED! %s\n", strerror(errno));
	else cout<< "	Task "<< tasks[index].pid<< " stopped"<< endl;
}

/*
* This function kills a specific process, identified by it's index and using the pid to send SIGKILL
* to the process. 

* Arguments:
*	-index: the index of the process the user wishes to terminate
*	-tasks: a vector of the struct task containing accepted tasks
*/
void terminate( int index, vector<task> &tasks){
	if (tasks.empty()) {
		cout<< "	No tasks have been accepted yet!"<< endl;
		return;
	}
	if (index >= tasks.size() || index < 0) {
		cout<< "	Index out of bounds!"<< endl;
		return;
	}
	
	pid_t pid = tasks[index].pid;
	
	tasks[index].terminated = TERMINATED;
	if (kill(pid, SIGKILL) < 0) 
		printf("SIGKILL FAILED! %s\n", strerror(errno));
	else cout<< "	Task "<< tasks[index].pid<< " terminated"<< endl;
}

/*
* This function continues a specific process that has been stopped, identified by it's index
* and using the pid to send SIGCONT to the process. 
*
* Arguments:
*	-index: the index of the process the user wishes to continue
*	-tasks: a vector of the struct task containing accepted tasks
*/
void cont(int index, vector<task> &tasks){
	if (tasks.empty()) {
		cout<< "	No tasks have been accepted yet!"<< endl;
		return;
	}
	if (index >= tasks.size() || index < 0) {
		cout<< "	Index out of bounds!"<< endl;
		return;
	}
	
	pid_t pid = tasks[index].pid;
	
	if (kill(pid, SIGCONT) < 0)
		printf("SIGCONT FAILED! %s\n", strerror(errno));
	else cout<< "	Task "<< tasks[index].pid<< " continued"<< endl;
}

/*
* The exit function will iterate through the vector of tasks and check if they have been explicitly
* terminated. If it has not, it will call kill to terminate it itself before closing the shell.
*
* Arguments:
*	-tasks: a vector of the struct task containing accepted tasks
*/
void exit(vector<task> &tasks){
	for(int i = 0; i < tasks.size(); i++){
		if (tasks[i].terminated != TERMINATED)
			terminate(i, tasks);
	}
}

/*
* This function will find out if the state of the process with the pid the user enters and print
* it's information. If it is terminated, it will only print the head information. If it is still
* running, it will print the formation of the head and any descendant procceses  it has.
*
* Arguments:
*	-token: the char array containing the parsed tokens of the command entered
*/
void check(char token[][MAXWORD]){
	FILE *processes;
	char path[MAXLINE];
	vector <string> children, pathSplit;

	//Create pipe and run ps command in the background
	if ((processes = popen("ps -u $USER -o user,pid,ppid,state,start,cmd --sort start", "r")) == NULL){
		printf("POPEN FAILED! %s\n", strerror(errno));
		exit(0);
	}
	
	bool terminated = false;
	bool found = false;
	string header;
	//Get the lines of the file popen generated and see if any match the pid
	while (fgets(path, sizeof(path)-1, processes) != NULL){
		string strPath = path;
		char * word = strtok(path, " ");
		
		/* 
		* Loop through the string to extract all tokens. Had to do this here because calls to
		* split were failing and not extracting string properly.
		*/
		while( word != NULL ) {
			pathSplit.push_back(word);
			word = strtok(NULL, " ");
		}
		
		//Capture the header line
		if (pathSplit[0] == "USER") header = strPath;
		
		/*
		* Check for head process. If it has terminated, we can print it's information and
		* stop searching the file. If not it is running, print and then find all descendants 
		* and print their information by checking the children vector for matching parents.
		*/
		if (pathSplit[1] == token[1]){
			found = true;
			
			if (pathSplit[3] == "Z"){
				terminated = true;
				printf("	target_pid = %s 	terminated\n\n", token[1]);
				cout<< header<< strPath<< endl;
				break;
			}
			else if (pathSplit[3] == "S"){
				printf("	target_pid = %s 	running\n\n", token[1]);
				cout<< header<< strPath;
			}
		}
		
		if (terminated == false){
			/* 
			* If it is a direct child of the head process (PPID = head PID), print and add 
			* it's PID to children vector.
			*/
			if (pathSplit[2] == token[1]){
				cout<< strPath;
				children.push_back(pathSplit[1]);
			}
			
			/*
			* If there is a child of the head process, check to see if the current line had a PPID
			* matching a descendant. If it does, print its information and also add it's PID to 
			* children.
			*/
			if (children.size() > 0){
				for (int i = 0; i < children.size(); i++){
					if (children[i] == pathSplit[2]){
						cout<< strPath;
						children.push_back(pathSplit[1]);
					}
				}
			}
		}
		
		// Clear pathSplit to get ready for next line
		pathSplit.clear();
	}
	
	if (pclose(processes) != 0) cout<< "PCLOSE FAILED!"<< endl;
	
	if (found == false) cout<< "	Invalid PID provided!"<< endl;
	if (terminated == false) cout<< endl;
	
}

/*
* This function starts the program and runs the main loop in which the user enters commands. 
* It will decide which function to call based on the command entered. 
*
* Returns:
	0 once the user uses the exit or quit command.
*/
int main (){
	vector <task> tasks;
	setLimit();
	
	// Get start time
	clock_t start;
	struct tms tmsStart = getTime(start);
	
	pid_t pid;
	pid = getpid();
	
	bool i = true; 
	while (i == true) {
		printf(" msh379 [%d]: ", pid);
		string command;
		getline(cin, command);
		
		char token [MAX_NTOKEN][MAXWORD];
		char delim [1] = {' '};
		
		int tokenNum = split(command, token, delim);
		
		if ((string) token[0] == "cdir"){
			if (tokenNum < 2) cout<< " Please provide a pathname!"<< endl;
			else if (tokenNum > 2) cout<< " Too many arguments provided!"<< endl;
			else{
				cdir(token);
			}
		}
		else if ((string) token[0] == "pdir"){
			if (tokenNum != 1) cout<< " Too many arguments provided!" << endl;
			else pdir();
		}
		else if ((string) token[0] == "lstasks"){
			if (tokenNum != 1) cout<< " Too many arguments provided!" << endl;
			else lstasks(tasks);
		}
		else if ((string) token[0] == "run"){
			//check if we have reached the task number maximum. If yes return to loop.
			if (tasks.size() == NTASK){
				cout<< "	Number of accepted tasks has reached the maximum amount. Please exit "<< endl;
				cout<< "	the shell and try again."<< endl;
				continue;
			}
			
			pid_t forkPid;
			if ((forkPid = fork()) < 0) cout<< "fork error"<< endl;
			else if (forkPid == 0){
				
				if (tokenNum < 2) cout<< " Please provide the program and any arguments!" << endl;
				else if (tokenNum > 6) cout << " A maximum of 4 program arguments are permitted!" << endl;
				else run(token, tokenNum);
			}
			else{
				/*
				* Sleep for a second in case there is output to make it cleaner and also to allow time 
				* for the parent to catch the child.
				*/
				sleep(1);
				int status;
				pid_t child;
				
				//Get status of child regardless if it is done or not
				if ((child = waitpid(forkPid, &status, WNOHANG)) == -1)
					cout<< "Waitpid failed!"<< endl;
				/*
				* If the child didn't execute properly and exited, continue without adding it to the 
				* task list.
				*/
				else if(WIFEXITED(status)) continue;
				else addTask(tasks, command, forkPid);
			}
		}
		else if ((string) token[0] == "stop"){
			if(tokenNum != 2) cout << " Please provide a single task number!" << endl;
			else stop(atoi(token[1]), tasks);
		}
		else if ((string) token[0] == "terminate"){
			if(tokenNum != 2) cout << " Please provide a single task number!" << endl;
			else terminate(atoi(token[1]), tasks);
		}
		else if ((string) token[0] == "continue"){
			if (tokenNum != 2) cout << " Please provide a single task number!" << endl;
			else cont(atoi(token[1]), tasks);
		}
		else if ((string) token[0] == "check"){
			if (tokenNum != 2) 
				cout << " Please provide only the pid of the process you wish to check!" << endl;
			else check(token);
		}
		else if ((string) token[0] == "exit"){
			exit(tasks);
			i = false;
		}
		else if ((string) token[0] == "quit"){
			i = false;
		}
		else{
			// If we have reached this point, an incorrect command has been made.
			cout<< "	Command not recognized. Please select and type one of the following commands:\n\n";
			cout<< "	cdir pathname:         change working directory. If HOME/... or $HOME/... is\n"<<
				   "	                       typed msh379 will assume the folowing files exist in the\n"<<
				   "	                       current working directory.\n";
			cout<< "	pdir:                  prints the current working directory.\n";
			cout<< "	lstasks:               lists the tasks that have not been terminated.\n";	
			cout<< "	run pgm arg1 ... arg4: Run a program with up to 4 arguments.\n";
			cout<< "	stop taskNo:           send a stop signal to the task in index taskNo.\n";
			cout<< "	continue taskNo:       send a continue signal to the task in index taskNo.\n";
			cout<< "	terminate taskNo:      send a kill signal to the task in index taskNo.\n";
			cout<< "	check target_pid:      check the status of a task with the pid target_pid.\n";
			cout<< "	exit:                  exit msh379 and terminate any processes not yet terminated.\n";
			cout<< "	quit:                  exit msh379 without terminating processes left unterminated.\n";
		}
	}
	
	
	// Get time after main loop
	clock_t end;
	struct tms tmsEnd = getTime(end);
	
	printTimes(start, end, &tmsStart, &tmsEnd);
	return 0;
}


