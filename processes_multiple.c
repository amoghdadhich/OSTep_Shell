# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
# include <sys/wait.h>
# include <ctype.h>

char* read_input()
{
    char* buffer;
    size_t len, chars_read;
    len = 32;

    buffer = (char *) malloc(len * sizeof(char));

    if (buffer == NULL)
    {
        printf("Unable to allocate memory for buffer/n");
        exit(1);
    }

    chars_read = getline(&buffer,&len,stdin);

    buffer[chars_read - 1] = '\0';      // replace newline with a NULL character
    return buffer;
}

char** slice_command(char* input_read)
{
     // Takes in the read input and slices up the spaces
    // Returns a pointer to an array of pointers (char**)
    // The array of pointers has each cell pointing to one slice of the inputted shell command
    // The returned array is of the form {"cmd", "arg1", "arg2"..."NULL"}

    char* chunk; // Pointer to each chunk of the string
    char * input_copy; // Copy of the input
    char** arg_list = malloc(sizeof(char*) * 32); // List of command arguments 

    input_copy = strdup(input_read); // Copy the input to another string so the original is not affected

    int i = 0;
    while((chunk = strsep(&input_copy, " ")) != NULL)
    {
        if(strcmp(chunk,"") == 0)  // if there are extra spaces we find empty chunks, skip the empty chunk
        {
            continue;
        }
        arg_list[i] = strdup(chunk);
        i++;
    }
    
    // add a null at the end of the args
    arg_list[i] = NULL;

    free(input_copy);
    free(chunk);

    // return a pointer to the list of pointers
    return arg_list;
}

void parse_command(char* input_read, char* path[])
// Parses a command by looking for it in the defined commands
// If not in the defined commands, look for it in the directories given by path
// path: array of strings, defining the various paths in which the command executables are located
// path/COMMAND is the executable file
{
    char** arg_list = slice_command(input_read);
    char buffer[1024];
    int i = 0;
    int access_flag = -1;

    // locate the path in which the command is available
    // Use the access system call to do this

    while (path[i] != NULL && access_flag != 0)
    {
        // check for execution access to the file path/command_name
        // buffer is a temporary variable to which we copy each path name
        // append the command to the path name

        strcpy(buffer, path[i]);
        strcat(buffer, "/");
        strcat(buffer, arg_list[0]);
       
        // check the access flag
        // repeat till we find the right path or we run out of paths to check
        access_flag = access(buffer, X_OK);
        i++;
    }
    
    if(access_flag == -1)
    {
        printf("Command not found in PATH\n");
        return;
    }

    execvp(buffer, arg_list);
}

int is_inbuilt(char* input_read)
{   
    char ** arg_list = slice_command(input_read);
    char * inbuilt_list[] = {"cd", "path", NULL};    // list of all inbuilt commmand names

    int i = 0, inbuilt_flag = 0;

    while(inbuilt_list[i]!= NULL)       // compare the inputted command with the inbuilt command names
    {
        // if the command is present in the list return with flag 1
        if(strcmp(inbuilt_list[i], arg_list[0]) == 0)
            {
            inbuilt_flag = 1;
            return inbuilt_flag;
            }
        i++;
    }

    // if command not found return with inbuilt flag 0
    return inbuilt_flag;
}

void modify_path(char* arg_list[], char* path[])
{
    int i = 1;

    while (arg_list[i]!= NULL)
    {
        path[i-1] = arg_list[i];
        i++;
    }

    path[i-1] = NULL;
}

char** multi_command(char* input_read)
{
    // Takes the input which has been read and slices it into separate commands separated by &
    // The spaces at the beginning and the end of each command are trimmed
    // The function returns an array of input commands to be processed

    char* chunk; // Pointer to each chunk of the string
    char* input_copy; // Copy of the input
    char** command_list = malloc(sizeof(char*) * 12); // List of commands, dynamically created with MAX size of 12 commands

    input_copy = strdup(input_read); // Copy the input to another string so the original is not affected
    
    int i = 0;
    char* end;
    while((chunk = strsep(&input_copy, "&")) != NULL)
    {
        command_list[i] = strdup(chunk);

        // trim spaces before string
        // while the first character is a space, keep moving the string pointer to the next character        
        while(isspace(command_list[i][0]))
        {
            command_list[i] = command_list[i] + 1;
        }
        
        end = command_list[i] + strlen(command_list[i]) - 1; // pointer to the last character of the string

        // trim space at the end of string
        while(end > command_list[i] && isspace((unsigned char)*end)) 
        {
            end--;
            end[1] = '\0'; // Character next to the last non space character is null
        }

        i++;
    }
    
    // add a null at the end of the args
    command_list[i] = NULL;
    
    
    // Free up the memory created to hold the copy of the input
    free(input_copy);
    free(chunk);

    // return a pointer to the list of pointers
    return command_list;
}

int main(int argc, char* argv[])
{
    printf("wish> ");
    char* input_read = read_input();  // read the input command

    // default path 
    // Can be modified to contain a list of valid paths
    char* path[10] = {"/bin", NULL};

    // Continue to run the shell till the user types "exit"
    while (strcmp(input_read, "exit") != 0)
    {

        // Split the input read based on any & present
        // Each section of this is a separate command

        char** command_list = multi_command(input_read);
        
        int i = 0, children = 0;
        char* input;

        while (command_list[i] != NULL)
        {
            input = strdup(command_list[i]); // input to be executed
            i++;
            
            // Execute inbuilt command in the parent process
            if(is_inbuilt(input) == 1)
            {
                char ** arg_list = slice_command(input);

                // Change directory command
                    if (strcmp(arg_list[0], "cd") == 0)
                    {
                        char s[100];
                        if(chdir(arg_list[1]) != 0)
                            perror("cd failed");
                        else
                            printf("Now in %s\n", getcwd(s, 100));
                    }
    
                // Change path variable command
                    if (strcmp(arg_list[0], "path") == 0)
                        {
                            modify_path(arg_list, path);
                        }
            }

            // Library commands
            else{
                children++; // keep count of number of child processes
                
                // Fork before executing library commands with exec
                int pid = fork();

                if(pid == 0)
                {
                    // if the command is not inbuilt, create a fork to execute it
                    // Execute the command by looking for it in the path variable
                    parse_command(input, path);
                }
            }
        }
                
        // Wait for all children to exit
        while(children > 0)
            {   
                wait(NULL);
                children--;
            }

        // Ready the shell to take new input
        printf("wish> ");
        input_read = read_input();
    }

       
}
