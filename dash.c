#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char error_message[30] = "An error has occured\n";
void exit_BI(void);
void cd_BI(char* directory);
void path_BI(char** args, char** paths);
void parallelProcessing(char** paths, char* inputLine, int temp_stdout, int temp_stderr);

int main(int argc, char** argv){
  
  //--------------------------------CALCULATING SHELL MODE---------------------//
  char* shellMode = "";

  //check if the user put too many arguments, return with failure
  if (argc > 2){
    write(STDERR_FILENO, error_message, strlen(error_message));
      return -1;
  }

  //check to see if the user wants to use a file or cmd prompt
  else if (argc == 2){
    shellMode = "batch";
  }
  else if (argc == 1){
    shellMode = "interactive";
  }

  //-------------------------SOME VARIABLE INITIALIZATIONS---------------------//

  //open the file provided by the user and prepare it for parsing
  FILE *batchFile;
  if (shellMode == "batch"){
    batchFile = fopen(argv[1], "r");
  }

  //create an array to hold all the paths the user and program will want to use, add /bin as a sample
  char* paths[128] = {"/bin"};

  //backup stdout and stderr so that we can switch back after redirection is finished
  int temp_stdout = dup(1);
  int temp_stderr = dup(2);

  //perform while loop
  while(1){

    //------------------GETTING USER INPUT------------------//

    //variables needed to use getline(), inputLine contains the actual input, use getline_results to test if it worked, also accounts for EOF error  
    char* inputLine = NULL;
    size_t bufferSize = 0;
    int getline_results = 0;

    //if the shell is in interactive mode, print the prompt and get the command from standard input
    if (shellMode == "interactive"){
      printf("dash>");
      int getline_results = getline(&inputLine, &bufferSize, stdin);
    }
    //if the shell is in batch mode, read the commands from the file opened earlier
    else if (shellMode == "batch"){
      int getline_results = getline(&inputLine, &bufferSize, batchFile);
      //check for end of file error
      if (getline_results == -1){
	exit(0);
      }
    }

    //check if there's an &, if there is, move to the parallel processing function, then move to the next iteration of while
    if (strstr(inputLine, "&") != NULL){
      parallelProcessing(paths, inputLine, temp_stdout, temp_stderr);
	continue;
    }

    //------------------TOKENIZATION--------------------------//

    //tokenize the input, initialize the variable that will hold it
    char* tokenString;
    //hold all the tokens. 128 should be enough space
    char* argTokens[128];

    //tokenize the first part, delimiter is the space
    tokenString = strtok(inputLine, " \n\t\r");
    //used to keep track of the position in the args array
    int index = 0;

    //finish the rest of the tokenization
    while(tokenString != NULL){
      argTokens[index] = tokenString;
      tokenString = strtok(NULL, " \n\t\r");
      index++;
    }

    //make last position null
    argTokens[index] = NULL;

    //---------------EXECUTE BUILT IN COMMANDS-------------------//
      
    //built in command variables
    char *exitString = "exit";
    char *cdString = "cd";
    char *pathString = "path";

    //perform the built in commands if they are called
    if (strcmp(argTokens[0], exitString) == 0){
      exit_BI();
    }
    else if (strcmp(argTokens[0], cdString) == 0){
      int i = 0;
      //check how many args there are
      for (i = 0; i < 128; i++){
        if (argTokens[i] == NULL){
	  write(STDERR_FILENO, error_message, strlen(error_message));
          break;
        }
      }
      //even though i starts at 0, NULL is accounted for as part of the arguments, so we don't need to do an i++
      //if there's 2 args, do the cd, otherwise don't
      if (i == 2){
        cd_BI(argTokens[1]);
      }
      //continue to next iteration of while loop 
      continue;
    }
    else if (strcmp(argTokens[0], pathString) == 0){
      path_BI(argTokens, paths);
      //continue to next iteration of while loop
      continue;
    }

    //--------------------------EXECUTE NON BUILT IN COMMANDS------------------//

    //variables  to hold the path command with the path name, boolean to break the loop, check if the access worked or not, and the path index to loop through the paths array
    char* path_cmd = "";
    int access_pass_fail = 0;
    int path_index = 0;

    //conduct path test to see if any of the paths are accessible. If they aren't, that means the path test failed
    for (path_index = 0; path_index < 127; path_index++){
      //if we're at the end of the path list before full iterations complete, exit the loop
      if (paths[path_index] == NULL){
        access_pass_fail = -1;
        break;
      }

      char test_path[256] = {0};

      //concatenate all the strings needed together to make the full path name
      strcat(test_path, paths[path_index]);
      strcat(test_path, "/");
      strcat(test_path, argTokens[0]);

      //see if the path is accessible, if it is use it, otherwise keep going
      access_pass_fail = access(test_path, X_OK);
      if (access_pass_fail == 0){
        argTokens[0] = test_path;
        break;
      }
    }

    //test to see if the path access worked, if it didn't, finish iteration of while loop and fetch the next command
    if (access_pass_fail < 0){
      write(STDERR_FILENO, error_message, strlen(error_message));
      continue;
    }
      
    pid_t pid = fork();

    //execute the command in the child process, pass the path to execv
    if (pid == 0){
      //check how many args there are
      int argnum = 0;
      for (argnum = 0; argnum < 128; argnum++){
        if (argTokens[argnum] == NULL){
          break;
        }
      }

      //check if the arguments want a redirection to a text file
      if (argnum > 2 && strstr(argTokens[argnum - 2], ">") != NULL && strstr(argTokens[argnum - 1], ".txt") != NULL){
	//open the file desired in write only mode
	int fd = open(argTokens[argnum - 1], O_TRUNC | O_WRONLY);
	
	if (fd < 0){
	  fd = open(argTokens[argnum - 1], O_TRUNC |  O_CREAT, 0666);
	}

	//change standard output and error to the file, then close the file since we don't need it anymore
	dup2(fd, 1);
	dup2(fd, 2);
	
	close(fd);

	//set the last 2 arguments to null, since we already know where to write to and don't need them anymore
	argTokens[argnum - 1] = NULL;
	argTokens[argnum - 2] = NULL;
      }

      execv(argTokens[0], argTokens);
      
      //set the default standard output and error again
      dup2(temp_stdout, 1);
      dup2(temp_stderr, 2);
    }
    //wait for the child process to die as the parent
    else if (pid > 0){
      wait(NULL);
    }
    //in case fork() screws up
    else if (pid < 0){
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(1);
    }
  }

  //program shouldn't reach it, but just in case, put a return statement
  return -1;
}

//-------------------BUILT IN COMMAND FUNCTIONS--------------------//

void exit_BI(void){
  exit(0);
}

void cd_BI(char* directory){
  int chdir_results = chdir(directory);
  if (chdir_results != 0){
    write(STDERR_FILENO, error_message, strlen(error_message));
  }
}

void path_BI(char* args[], char* paths[]){
  int i = 0;
  //reset the paths to null
  for (i = 0; i < 128; i++){
    paths[i] = NULL;
  }

  //fill in the empty paths array with the new paths from the user
  for (i = 1; i < 128; i++){
    if (args[i] == NULL){
      break;
    }
    else{
      paths[i - 1] = args[i];
    }
  }


  //this code block was supposed to add paths to the paths[] array, replaced it with code that replaces previous paths with new paths
  /*
  //find next spot to put the paths
  for (i = 0; i < 128; i++){
    //if it's NULL, put it there
    if (paths[i] == NULL){
      int j = 0;
      
      //new for loop for going through args[] and adding paths accordingly
      for (j = 1; j < 128; j++){
	//check if we're at the end of the user's input, then break out of this for loop
	if (args[j] == NULL){
	  break;
	}
	else{
	  //make sure we're not going over the path coverage
	  if (i == 128){
	    break;
	  }
	  paths[i] = args[j];
	  //won't be using the first for loop anymore, so manually use i as we need
	  i++;
	}
      }

      //break out of the first for loop
      break;
    }
    //no need for else, just continue with the next
    }*/
}

//--------------------------PARALLEL PROCESSING-----------------------//

//lots of copy paste with some minor adjustments
//essential logic is tokenize by using & as a delimiter, then use a for loop to run each of the tokens by repeating the whole process done in main
void parallelProcessing(char* paths[], char* inputLine, int temp_stdout, int temp_stderr){
 
  //------------------TOKENIZATION--------------------------//

  //tokenize the input, initialize the variable that will hold it
  char* tokenString;
  //hold all the tokens. 128 should be enough space
  char* argTokens[128];

  //tokenize the first part, delimiter space replaced by &
  tokenString = strtok(inputLine, "\n\t\r&");
  //used to keep track of the position in the args array
  int index = 0;

  //finish the rest of the tokenization
  while(tokenString != NULL){
    argTokens[index] = tokenString;
    tokenString = strtok(NULL, "\n\t\r&");
    index++;
  }

  //make last position null
  argTokens[index] = NULL;
  
  int i = 0;
  //start looping through the argTokens[] array
  for (i = 0; i < 128; i++){
    if (argTokens[i] == NULL){
      break;
    }

    //------------------TOKENIZATION--------------------------//

    //tokenize the input, initialize the variable that will hold it
    char* tokenString;
    //hold all the tokens. 128 should be enough space
    char* subargTokens[128];

    //tokenize the first part, delimiter is the space, since we already separated them by &
    tokenString = strtok(argTokens[i], " \n\t\r");
    //used to keep track of the position in the args array
    int index2 = 0;

    //finish the rest of the tokenization
    while(tokenString != NULL){
      subargTokens[index2] = tokenString;
      tokenString = strtok(NULL, " \n\t\r");
      index2++;
    }

    //make last position null
    subargTokens[index2] = NULL;

    for (index2 = 0; index2 < 128; index2++){
      if (subargTokens[index2] == NULL){
	break;
      }
    }

    //---------------EXECUTE BUILT IN COMMANDS-------------------//

    //built in command variables
    char *exitString = "exit";
    char *cdString = "cd";
    char *pathString = "path";

    //perform the built in commands if they are called
    if (strcmp(subargTokens[0], exitString) == 0){
      exit_BI();
    }
    else if (strcmp(subargTokens[0], cdString) == 0){
      int i = 0;
      //check how many args there are
      for (i = 0; i < 128; i++){
        if (subargTokens[i] == NULL){
	  write(STDERR_FILENO, error_message, strlen(error_message));
          break;
        }
      }
      //even though i starts at 0, NULL is accounted for as part of the arguments, so we don't need to do an i++
      //if there's 2 args, do the cd, otherwise don't
      if (i == 2){
        cd_BI(subargTokens[1]);
      }
      //continue to next iteration of while loop
      continue;
    }
    else if (strcmp(subargTokens[0], pathString) == 0){
      path_BI(subargTokens, paths);
      //continue to next iteration of while loop
      continue;
    }

    //--------------------------EXECUTE NON BUILT IN COMMANDS------------------//

    //printf("PID: %d", pid);
    //vraibles to hold the path command with the path name, boolean to break the loop, check if the access worked or\
    //not, and the path index to loop through the paths array
    char* path_cmd = "";
    int access_pass_fail = 0;
    int path_index = 0;

    //conduct path test to see if any of the paths are accessible. If they aren't, that means the path test failed
    for (path_index = 0; path_index < 127; path_index++){
      //if we're at the end of the path list before full iterations complete, exit the loop
      if (paths[path_index] == NULL){
        access_pass_fail = -1;
        break;
      }

      char test_path[256] = {0};

      //concatenate all the strings needed together to make the full path name
      strcat(test_path, paths[path_index]);
      strcat(test_path, "/");
      strcat(test_path, subargTokens[0]);

      //see if the path is accessible, if it is use it, otherwise keep going
      access_pass_fail = access(test_path, X_OK);
      if (access_pass_fail == 0){
        subargTokens[0] = test_path;
        break;
      }
    }

    //test to see if the path access worked, if it didn't, finish iteration of while loop and fetch the next command
    if (access_pass_fail < 0){
      write(STDERR_FILENO, error_message, strlen(error_message));
      continue;
    }

    pid_t pid = fork();

    //execute the command in the child process, pass the path to execv
    if (pid == 0){
      //check how many args there are
      int argnum = 0;
      for (argnum = 0; argnum < 128; argnum++){
        if (subargTokens[argnum] == NULL){
          break;
        }
      }

      //check if the arguments want a redirection to a text file
      if (argnum > 2 && strstr(subargTokens[argnum - 2], ">") != NULL && strstr(subargTokens[argnum - 1], ".txt") != NULL)\
      {
	//open the file desired in write only mode
        int fd = open(subargTokens[argnum - 1], O_TRUNC | O_WRONLY);

        if (fd < 0){
          fd = open(subargTokens[argnum - 1], O_TRUNC |  O_CREAT, 0666);
        }

        //change standard output and error to the file, then close the file since we don't need it anymore
        dup2(fd, 1);
        dup2(fd, 2);

        close(fd);

        //set the last 2 arguments to null, since we already know where to write to and don't need them anymore
        subargTokens[argnum - 1] = NULL;
        subargTokens[argnum - 2] = NULL;
      }

      execv(subargTokens[0], subargTokens);

      //set the default standard output and error again
      dup2(temp_stdout, 1);
      dup2(temp_stderr, 2);
    }
    //wait for the child process to die as the parent
    else if (pid > 0){
      wait(NULL);
    }
    //in case fork() screws up
    else if (pid < 0){
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(1);
    }
  }
}
