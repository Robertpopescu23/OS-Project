/*
This program should analyse in the directories passed as command line arguments(which is formed a process for every file opening and they will be opened recursively),
will analyse all files check if those are regular files and then check if among them are malicious files.

1.First we check the permission of the file and if the permission is missing a dedicated process will be created to perform a syntetic analyse of the script: verify_for_malicious.sh.
Afterwards it should be isolated in a safe environment.

2.Second we perform a syntatic analyse of its content to identify signs of malicious intent or corruption.
The script will include checking the nb of lines, words, characters, in the file as well as searching for keywords with corrupted or malicious files, such as: corrupted, dangerous, risk, attack, malware, malicious or even nonASCII characters.

3.Files indentified as dangerous will be moved to a special directory specified in the command line arguments named: isolated_space_dir.

Command line arguments shoudl look like this: ./milestone4 -o output_dir -s izolated_space_dir dir1 dir2 dir3

Each directory from dir1, dir2, dir3 should have a child process that opens each recursively and checks for the files

argv[0] -> ./milestone4
argv[1] -> "-o"
argv[2] -> output_dir
argv[3] -> "-s"
argv[4] -> isolated_space_dir
argv[5] -> dir1
argv[6] -> dir2
argv[7] -> dir3

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h> 
#include <libgen.h>

#define MAX_PATH_LENGTH 1024
#define BUFFER_SIZE 4096

char* malicious_words_matrix[] = {"corrupted", "dangerous", "risk", "attack", "malware", "malicious"}; 

int Check_File_Permission(const char* path, const struct stat* st)
{
	if((!(st -> st_mode)) & (S_IRUSR))
	{
		perror("\nError: File is not readable by owner!\n");
		return -1;
	}
	if((!(st -> st_mode)) & (S_IWUSR))
	{
		perror("\nError: File is not writable by owner!\n");
        return -1;
	}
    if((!(st->st_mode)) & (S_IXUSR)) {
        perror("\nError: File is not executable by owner.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IRGRP)) {
        perror("\nError: File is not readable by group.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IWGRP)) {
        perror("\nError: File is not writable by group.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IXGRP)) {
        perror("\nError: File is not executable by group.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IROTH)) {
        perror("\nError: File is not readable by others.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IWOTH)) {
        perror("\nError: File is not writable by others.\n");
        return -1;
    }
    if((!(st->st_mode)) & (S_IXOTH)) {
        perror("\nError: File is not executable by others.\n");
        return -1;
    }
    return 0;
}

int Check_File_Name(const char* filename)
{
    for(int index = 0; malicious_words_matrix[index] != NULL; index++)
    {
        if(strstr(filename, malicious_words_matrix[index]) != NULL)
        {
            perror("Malicious file detected, move it to the isolated directory!\n");
            return -1;
        }
    }
    for(int index = 0; filename[index] != '\0'; index++)//This iteration also cheks for NON-ASCII characters in the file name
    {
        if(filename[index] < 32 || filename[index] > 126)
        {
            return -1;
        }
    }
    return 0;
}

int Copy_File(const char* source_path, const char* destination_path) {
    int source_folder = open(source_path, O_RDONLY);
    if (source_folder == -1) {
        perror("Error opening the source file for reading!\n");
        return -1;
    }
    char* destination_dir = dirname(strdup(destination_path));
    if (mkdir(destination_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
        perror("Error creating destination directory!\n");
        close(source_folder);
        return -1;
    }
    int destination_folder = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (destination_folder == -1) {
        perror("Error opening destination file for writing");
        close(source_folder);
        return -1;
    }
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(source_folder, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(destination_folder, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error writing to destination file");
            close(source_folder);
            close(destination_folder);
            return -1;
        }
    }
    if (bytes_read == -1) {
        perror("Error reading from source file");
        close(source_folder);
        close(destination_folder);
        return -1;
    }
    close(source_folder);
    close(destination_folder);
    return 0;
}

int Analyse_Malicious_File(const char* malicious_file)
{
    const char* script_path = "/Users/popescurobert/Desktop/OS_Project/isolated_space_dir/verify_for_malicious.sh";
    size_t command_length = strlen(script_path) + strlen(malicious_file) + 2;// \0 for both strings terminator
    char* command = (char*)malloc(command_length);
    if(command == NULL)
    {
        perror("Error allocating memory for command of executing shell script file!\n");
        return -1;
    }
    snprintf(command, command_length, "%s %s", script_path, malicious_file);
    int status = system(command);
    if(status == -1) {
        perror("Error executing the shell script!\n");
        free(command);
        return -1;
    } else if(WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if(exit_status == 0) {
            printf("Shell script executed successfully\n");
        } else {
            printf("Shell script exited with status %d\n", exit_status);
        }
    } else {
        printf("Shell script terminated anormally\n");
    }
    free(command);

    return 0;
}

void Process_Directory_Recursive(const char* source_dir, const char* destination_dir, const char* isolation_space_dir) {
    DIR* dir = opendir(source_dir);
    if (dir == NULL) {
        perror("Error opening source directory!\n");
        exit(-1);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore special directories "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_path[MAX_PATH_LENGTH];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);

        char destination_path[MAX_PATH_LENGTH];
        snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_dir, entry->d_name);

        char isolation_space_path[MAX_PATH_LENGTH];
        snprintf(isolation_space_path, sizeof(isolation_space_path), "%s/%s", isolation_space_dir, entry -> d_name);
        struct stat st;
        if(stat(source_path, &st) != 0)
        {
            perror("Error getting file info!\n");
        }
        if(S_ISREG(st.st_mode))//Checking for regular files
        {
            int malicious_file_flag = 0;
            if(Check_File_Permission(source_path, &st) != 0)
            {
                malicious_file_flag = 1;
                //A dedicated process will be hold
                if(Copy_File(source_path, isolation_space_path) == -1)
                {
                    perror("Error copying the isolated file!\n");
                    continue;
                }
                //Else it should copy the malicious file

                //Calling the analysing function for the malicious files!
                if(Analyse_Malicious_File(source_path) != 0)
                {
                    perror("Unable to analyse the malicious file!\n");
                    continue;
                }
            }
            if(Check_File_Name(entry -> d_name) != 0)
            {
                malicious_file_flag = 1;
                if(Copy_File(source_path, isolation_space_path) == -1)
                {
                    perror("Error copying the isolated file!\n");
                    continue;
                }
                //Else it should copy the malicious file

                if(Analyse_Malicious_File(source_path) != 0)
                {
                    perror("Unable to analyse the malicious file!\n");
                    continue;
                }
            }
            if(!malicious_file_flag)//if it is still 0 will copy the regular file to the output_dir
            {
                if(Copy_File(source_path, destination_path) == -1)
                {
                    perror("Error copying the regular file!\n");
                    continue;
                }
            }
        }
        else if(S_ISDIR(st.st_mode))
        {
            //If it's directory opens recursively
            Process_Directory_Recursive(source_path, destination_path, isolation_space_path);
        }
    }
    closedir(dir);
}


int main(int argc, char* argv[]) {
    if (argc < 4) {
        perror("Error, not enough command line arguments, try again!\n");
        exit(EXIT_FAILURE);
    }

    char* output_dir = NULL;
    char* isolated_space_dir = NULL;

    // Parse command line arguments
    for(int index = 1; index < argc; index++) {
        if(strcmp(argv[index], "-o") == 0) {
            if(index + 1 >= argc) {
                perror("Error, output directory to store regular files and directories is not given as command line argument!\n");
                exit(EXIT_FAILURE);
            }
            struct stat st;
            if (stat(argv[index + 1], &st) != 0 || !S_ISDIR(st.st_mode)) {
                perror("Error, output directory is not a directory!\n");
                exit(EXIT_FAILURE);
            }
            output_dir = argv[index + 1];
        }
        else if(strcmp(argv[index], "-s") == 0) {
            if(index + 1 >= argc) {
                perror("Error, directory for isolating malicious files is not quite a directory!\n");
                exit(EXIT_FAILURE);
            }
            struct stat st;
            if (stat(argv[index + 1], &st) != 0 || !S_ISDIR(st.st_mode)) {
                perror("Error, directory for isolating malicious files is not a directory!\n");
                exit(EXIT_FAILURE);
            }
            isolated_space_dir = argv[index + 1];
        }
    }

    if (output_dir == NULL || isolated_space_dir == NULL) {
        perror("Error, output directory or isolated space directory is missing!\n");
        exit(EXIT_FAILURE);
    }

    //Code for processes
    pid_t processes[argc - 3]; //matrix to store all processes 
    int process_count = 0;
    for(int index = 3; index < argc; index++) {
        int pid = fork(); //creates a new process
        if(pid == -1) {
            perror("Error creating a process!\n");
            exit(EXIT_FAILURE);
        }
        else if(pid == 0) {
            Process_Directory_Recursive(argv[index], output_dir, isolated_space_dir);
            exit(EXIT_SUCCESS);
        }
        else {
            processes[process_count++] = pid;
        }
    }
    //Waiting for every process to finish
    for(int index = 0; index < process_count; index++) {
        waitpid(processes[index], NULL, 0);
    }
    printf("Processes created: \n");
    printf("Parent process PID: %d\n", getpid());
    for(int index = 0; index < process_count; index++) {
        printf("Child process: %d: PID: %d\n", index + 1, processes[index]);
    }
    return EXIT_SUCCESS;
}




