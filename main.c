#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

//awful c pre processor stuff for concatentating text number with string
#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

#define CRASH_CALL(call, label, line, code) if(call) {perror(label line);exit(code);}

size_t deadChildren;
size_t numChildren;
pid_t* childPIDs;

//sigchld handler
void sigchld_handler(int signo) {
	pid_t pid;
	int stat;
	//process child changes
	while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		//printf("PID %d has terminated with status %d.\n", pid, WEXITSTATUS(stat));
		for(int i = 0; i < numChildren; ++i) {
			if(childPIDs[i] == pid) {
				childPIDs[i] = -1;
				deadChildren += 1;
			}
		}
	}
}

void print_usage() {
	printf("Usage: sinmux [-s SELECTOR SEPARATOR] [-i INPUT SEPARATOR] [-u DELAY FROM STARTING COMMANDS TO SENDING INPUT IN US] [-d DELAY BETWEEN SENDING INPUTS IN US] COMMAND1 [COMMAND2] ...\n");
}

int main(int argc, char** argv) {
	//parse command line args
	int delim = '\n';
	char* selectorSeparator = ",";
	useconds_t upDelay = 1000000;
	useconds_t inputDelay = 10000;
	int c;
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{0,			0,					0,	0 }
		};
		c = getopt_long(argc, argv, "s:i:u:d:", long_options, &option_index);
		if (c == -1) {
			break;
		}
		
		switch (c) {
			case 0:
				printf("case 0\n");
				break;
			case 's':
				//printf("case s %s\n", optarg);
				if(strlen(optarg) == 1) {
					selectorSeparator = optarg;
				} else {
					print_usage();
					return 1;
				}
				break;
			case 'i':
				//printf("case i %s\n", optarg);
				if(strlen(optarg) == 1) {
					delim = optarg[0];
				} else {
					print_usage();
					return 1;
				}
				break;
			case 'u':
				//printf("case i %s\n", optarg);
				if(strlen(optarg) > 0) {
					upDelay = (int)strtol(optarg, NULL, 10);
					if(errno == ERANGE) {
						return 1;
					}
				} else {
					print_usage();
					return 1;
				}
				break;
			case 'd':
				//printf("case i %s\n", optarg);
				if(strlen(optarg) > 0) {
					inputDelay = (int)strtol(optarg, NULL, 10);
					if(errno == ERANGE) {
						return 1;
					}
				} else {
					print_usage();
					return 1;
				}
				break;
			default:
				print_usage();
				return 2;
				//printf("?? getopt returned character code 0%o ??\n", c);
		}
	}
	
	//set child handler
	deadChildren = 0;
	struct sigaction act = { 0 };
	act.sa_handler = &sigchld_handler;
	CRASH_CALL(sigaction(SIGCHLD, &act, NULL) == -1, "sigaction", S__LINE__, 5);
	
	//run shell commands and store write ends of pipes to shell commands' stdins
	//printf("option index %d\n", optind);
	int* outputFDs = malloc(sizeof(int)*(argc-optind));
	CRASH_CALL(outputFDs == NULL, "malloc", S__LINE__, 5);
	numChildren = (argc-optind);
	childPIDs = malloc(sizeof(pid_t)*(argc-optind));
	CRASH_CALL(childPIDs == NULL, "malloc", S__LINE__, 5);
	
	printf("example input for settings:\n%lu%s%s%c", numChildren-1, selectorSeparator, "input", delim);
	
	for(int i = 0; i < numChildren; ++i) {
		int pipefd[2];
		int ret;
		CRASH_CALL(pipe(pipefd) == -1, "pipe", S__LINE__, 5);
		CRASH_CALL((ret = fork()) < 0, "fork", S__LINE__, 5);
		if(ret == 0) {
			close(pipefd[1]);
			CRASH_CALL(dup2(pipefd[0], STDIN_FILENO) == -1, "dup2", S__LINE__, 5);
			CRASH_CALL(execl("/bin/bash", "bash", "-c", argv[optind+i], NULL) == -1,"execl", S__LINE__, 5);
		} else if(ret > 0){
			outputFDs[i] = pipefd[1];
			childPIDs[i] = ret;
			CRASH_CALL(close(pipefd[0]) == -1, "close", S__LINE__, 5);
		}
	}
	CRASH_CALL(usleep(upDelay) == -1, "usleep", S__LINE__, 5);
	//read lines and direct to designated output
	char* input = NULL;
	char* line = NULL;
	size_t size = 0;
	size_t inputSize = 0;
	//size_t selectorSize = 0;
	while(deadChildren < numChildren) {
		//printf("reading input\n");
		if (getdelim(&line, &size, delim, stdin) == -1) {
			if(feof(stdin)) {
				break;
			}
			perror("failed to read line ffd");
			break;
		} else {
			char* substr = strstr(line, selectorSeparator);
			if(substr == NULL) {
				printf("input of:\n%s\n", line);
				printf("invalid input format, input: [SELECTOR NUMBER][SELECTOR SEPARATOR][INPUT][INPUT DELIMITER]\n");
				break;
			} else {
				substr[0] = '\0';
				input = substr+1;
				//selectorSize = strlen(line);
				inputSize = strlen(input);
				int selector = atoi(line);
				//printf("Read selector %d input %.*s", atoi(line), inputSize, input);
				if(selector < 0 || selector > numChildren-1) {
					printf("input of:\n%s\n", line);
					printf("invalid input format, selector out of range input: [SELECTOR NUMBER][SELECTOR SEPARATOR][INPUT][INPUT DELIMITER]\n");
					break;
				}
				if(childPIDs[selector] == -1) {
					CRASH_CALL(close(outputFDs[selector]) == -1, "close", S__LINE__, 5);
					//outputFDs[selector] == -1;
					continue;
				}
				CRASH_CALL(write(outputFDs[selector], input, inputSize) == -1, "write", S__LINE__, 5);
				CRASH_CALL(usleep(inputDelay) == -1, "usleep", S__LINE__, 5);
			}
		}
		//printf("read an input\n");
	}
	
	for(int i = 0; i < numChildren; ++i) {
		if(outputFDs[i] != -1) {
			CRASH_CALL(close(outputFDs[i]) == -1, "close", S__LINE__, 5);
		}
		if(childPIDs[i] != -1) {
			CRASH_CALL(kill(childPIDs[i], SIGTERM) == -1, "kill", S__LINE__, 5);
		}
	}
	free(outputFDs);
	free(childPIDs);
}
