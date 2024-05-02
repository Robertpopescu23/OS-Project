#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>


#define MAX_PATH_LENGTH 1024 //max path for directory name
#define BUFFER_SIZE 4096



int Is_Regular_File(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("Error, cannot access info of the file => not a regular file!\n");
        return -1;
    }
    return S_ISREG(st.st_mode);
}

void Create_File_Snapshot(const char *original_path) {
    char snapshot_path[MAX_PATH_LENGTH];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s_snapshot.txt", original_path);
    int original_fd = open(original_path, O_RDONLY);
    if (original_fd == -1) {
        perror("Error opening original file for reading!\n");
        exit(EXIT_FAILURE);
    }
    int snapshot_fd = open(snapshot_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (snapshot_fd == -1) {
        perror("Error, snapshot for respective file was not created!\n");
        close(original_fd);
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(original_fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_written = write(snapshot_fd, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("Error writing to snapshot file.\n");
            close(original_fd);
            close(snapshot_fd);
            exit(EXIT_FAILURE);
        }
    }
    if (bytes_read == -1) {
        perror("Error reading from original file.\n");
        close(original_fd);
        close(snapshot_fd);
        exit(EXIT_FAILURE);
    }
    close(original_fd);
    close(snapshot_fd);
}

void Process_Directory_Recursive(const char* dirname)
{
    DIR* dir;
    struct dirent *entry;
    dir = opendir(dirname);
    if(dir == NULL)
    {
        perror("Error, the function cannot open if it's not a directory!\n");
        exit(EXIT_FAILURE);
    }
    while((entry = readdir(dir)) != NULL)
    {
        char fullPath[MAX_PATH_LENGTH];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirname, entry -> d_name);
        if(Is_Regular_File(fullPath))
        {
            printf("%s is a regular file.\n", fullPath);
            Create_File_Snapshot(fullPath);
            char snapshot_path[MAX_PATH_LENGTH];
            snprintf(snapshot_path, sizeof(snapshot_path), "%s_snapshot.txt", fullPath);
            Create_File_Snapshot(snapshot_path);
        }
        else if(entry -> d_type == DT_DIR && (strcmp(entry -> d_name, ".") != 0) && (strcmp(entry -> d_name, "..") != 0))
        {
            //Recursive calls for subdirectories
            Process_Directory_Recursive(fullPath);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[])
{
    if(argc > 10)
    {
        perror("Command line arguments are exceeded, try again!\n");
        exit(EXIT_FAILURE);
    }
    //Check each argument if it is a directory
    for(int index = 1; index < argc; index++)
    {
        struct stat st;//calling the struct stat for checking if it's directory
        if(stat(argv[index], &st) == -1)
        {
            perror("Error, given argument type is not a directory!\n"); 
            continue;
        }
        if(!S_ISDIR(st.st_mode))//Checks S_ISDIR if the following file is a directory or not and st.st_mode is from struct stat that checks if the st hold the file mode and file type returned by argv[index]
        {
            perror("The following arguemnts is not  a directory!\n");
            return(-1); //the same as EXIT_FAILURE;
        }

    }
    for(int index = 1; index < argc; index++)
    {
        Process_Directory_Recursive(argv[index]);
    }
    return EXIT_SUCCESS;
}
