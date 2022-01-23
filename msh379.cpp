#include <string>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>
#include <iostream>

using namespace std;
static int NTASK = 32;

/*
* This function will set the CPU time limit to the values specified in seconds. The soft limit will be 8 minutes and 
* the hard limit will be 10 miniutes. 
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
struct tms getTime(){
	struct tms timeStruct;
	clock_t time;
	
	if ((time = times(&timeStruct)) == -1) // Capture times
		cout<< "times error"<< endl;
	
	return timeStruct;
}

int main (){
	setLimit();
	
	// Get start time
	struct tms start = getTime();
	
	pid_t pid;
	fork();
	pid = getpid();
	printf("%d \n", pid);
	
	
	
	// Get time after main loop
	struct tms end = getTime();
	return 0;
}


