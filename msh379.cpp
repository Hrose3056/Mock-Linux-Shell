#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <signal.h>

#define NTASK 32
#define MAXWORD 32
#define MAXLINE 132
#define MAX_NTOKEN MAXWORD
#define TERMINATED 0
#define NOT_TERMINATED 1

using namespace std;

typedef struct task {
	string command;
	int index;
	pid_t pid;
	int terminated;
} task;

/*
* This function will take in a string and split it based on the field delimiters
* provided. It will fill the outToken array with the seperated strings and return
* the total number of tokens created.
*/
int split(string inStr, char token[][MAXWORD], char fieldDelim[]){
	int i, count;
	char *tokenp;
	
	count = 0;
	
	for (i = 0; i < MAX_NTOKEN; i++)
		memset(token[i], 0 , sizeof(token[i]));
	
	string inStrCpy = inStr; //create a copy of the string passed to the function
	if((tokenp = strtok(&inStr[0], fieldDelim)) == NULL) return 0; //return 0 if no token is found
	
	//stroe first token if found in if statement above
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
* This function will search a string for characters that are in the omitSet and
* then replace them with the string repStr. If the first character of the omitSet
* is '^', then only replace the first character.
*/ 	
int gsub(string &t, char *omitSet, char *repStr){
	int i, j, ocSeen, match, nmatch;
	string outStr;
	
	enum {MBEGIN = 1, MALL = 2} task;
	task = (omitSet[0] == '^')? MBEGIN : MALL; //see if we need to do first letter or whole string
	
	nmatch = 0;
	ocSeen;
	for(i = 0; i < t.size(); i++){
		//if we only need to get rid of the first letter, just set out string to the substring of t
		if((task == MBEGIN) && (ocSeen == 1)){
			outStr = t.substr(1, t.size() - 1);
			continue;
		}
		
		match = 0;
		for (j = 0; j < strlen(omitSet); j++){
			//Search to see if char is in the omitSet, if yes, add the replacement to the out string instead
			if (t[i] == omitSet[j]){
				match = 1;
				nmatch++;
				
				if (repStr[0] != NULL) outStr.push_back(repStr[0]);
				break;
			}
		}
		
		//if a match is not found, push the character to the back of the out string
		if (match == 0) {
			ocSeen = 1;
			outStr.push_back(t[i]);
		}
	}
	
	t = outStr;
	return nmatch;		
}


/*
* This function will set the CPU time limit to the values specified in 
* seconds. The soft limit will be 8 minutes and the hard limit will be 
* 10 miniutes. 
*/
void setLimit(){
	struct rlimit lim;
	lim.rlim_cur = 480; //set soft limit to 8 minutes
	lim.rlim_max = 600; //set hard limit to 10 minutes
	
	if (setrlimit(RLIMIT_CPU, &lim) == -1)
		fprintf(stderr, "%d\n", errno); //FIX

}

/*
* The getTime() function will capture the times of the user CPU, system CPU, and the terminated children. It stores it
* in the tms struct provided by sys/time.h and returns it.
*/
struct tms getTime(clock_t &time){
	struct tms timeStruct;
	
	if ((time = times(&timeStruct)) == -1) // Capture times
		cout<< "times error"<< endl;
	
	return timeStruct;
}

/*
* The printTimes function will print the times in seconds of the total time, the 
* msh379 system and user times, and the system and user times of the children 
* processes.
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
* The cdir function will change the working directory to a pathname specified
* by the user.
*/
void cdir(char token[][MAXWORD]){
	if (chdir(token[1]) < 0) 
		cout<< "	chdir to " << token[1]<< " failed" << endl;
	cout<< "	chdir to "<< token[1]<< " successful" << endl;
}

/*
* This function will print the current working directory.
*/
void pdir(char token[][MAXWORD]){
	char ptr[1024];

	if (getcwd(ptr, 1024) == NULL)
		cout<< "	getcwd failed"<< endl;
	else cout<< "	cwd: "<< ptr<< endl;
}

/*
* The lstasks function will print the pid, index, and commands enteres for all accepted tasks
* that have not been terminated by the user. 
*/
void lstasks(vector <task> tasks){
	for (int i = 0; i < tasks.size(); i++){
		if (tasks[i].terminated != TERMINATED)
			printf("	%d:   (pid: %d, command entered: %s)\n", tasks[i].index, tasks[i].pid, &(tasks[i].command)[0]);
	}
}

/*
* This function will attempt to run a command with up to 4 arguments. If successful, it will return 1.
* If unsuccessful it will return 0.
*/
int run(char token[][MAXWORD], int tokenNum){
	
		if (tokenNum == 2){
			if(execlp(token[1] , token[1], (char *) NULL) < 0){
				cout<< "execlp error"<< endl;
				return 0;
			}
		}
		if (tokenNum == 3){
			if(execlp(token[1], token[1], token[2], (char *) NULL) < 0){
				cout<< "execlp error"<< endl;
				return 0;
			}
		}
		if (tokenNum == 4){
			if(execlp(token[1], token[1], token[2], token[3], (char *) NULL) < 0){
				cout<< "execlp error"<< endl;
				return 0;
			}	
		}
		if (tokenNum == 5){
			if(execlp(token[1], token[1], token[2], token[3], token[4], (char *) NULL) < 0){
				cout<< "execlp error"<< endl;
				return 0;
			}
		}
		if (tokenNum == 6) {
			if(execlp(token[1], token[1], token[2], token[3], token[4], token[5], (char *) NULL) < 0){
				cout<< "execlp error"<< endl;
				return 0;
			}
		}
	
}

/*
* The addTask function will add a task to the list and create the task struct with the
* information of that task. 
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

void stop(int index, vector<task> &tasks){
	pid_t pid = tasks[index].pid;
	
	//signal(SIGSTOP, pid);
}

void kill( int index, vector<task> &tasks){
	pid_t pid = tasks[index].pid;
	
	kill(pid, SIGKILL);
}

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
		
		char* replacement = " ";
		char* omit = "$";
		
		int replaced = gsub(command, omit, replacement);
		int tokenNum = split(command, token, delim);
		
		if ((string) token[0] == "cdir"){
			if (tokenNum < 2) cout<< " Please provide a pathname!"<< endl;
			else if (tokenNum > 2) cout<< " Too many arguments provided!"<< endl;
			else{
				cdir(token);
			}
		}
		if ((string) token[0] == "pdir"){
			if (tokenNum != 1) cout<< " Too many arguments provided!" << endl;
			else pdir(token);
		}
		if ((string) token[0] == "lstasks"){
			if (tokenNum != 1) cout<< " Too many arguments provided!" << endl;
			else lstasks(tasks);
		}
		if ((string) token[0] == "run"){
			//check if we have reached the task number maximum. If yes return to loop.
			if (tasks.size() == NTASK){
				cout<< "	Number of accepted tasks has reached the maximum amount. Please exit "<< endl;
				cout<< "	the shell and try again."<< endl;
				continue;
			}
			
			pid_t forkPid;
			int status;
			if ((forkPid = fork()) < 0) cout<< "fork error"<< endl;
			else if (forkPid == 0){
				
				if (tokenNum < 2) cout<< " Please provide the program and any arguments!" << endl;
				else if (tokenNum > 6) cout << " A maximum of 4 program arguments are permitted!" << endl;
				else run(token, tokenNum);
			}
			else{
				addTask(tasks, command, forkPid);
				wait(&status);
			}
		}
		if ((string) token[0] == "stop"){
			if(tokenNum != 2) cout<< " Please provide a single task number!" << endl;
			else stop(atoi(token[1]), tasks);
		}
		if ((string) token[0] == "kill"){
			if(tokenNum != 2) cout<< " Please provide a single task number!" << endl;
			else kill(atoi(token[1]), tasks);
		}
		
		if ((string) token[0] == "quit"){
			i = false;
		}
	}
	
	
	// Get time after main loop
	clock_t end;
	struct tms tmsEnd = getTime(end);
	
	printTimes(start, end, &tmsStart, &tmsEnd);
	return 0;
}


