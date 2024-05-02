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

int Compare_Snapshots(const char* original_path)
{
    char snapshot_path[MAX_PATH_LENGTH];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s_snapshot.txt", original_path);
    int original_folder = open(original_path, O_RDONLY);
    int snapshot_folder = open(snapshot_path, O_RDONLY);
    if(original_folder == -1 || snapshot_folder == -1)
    {
        perror("Error opening the file for comparison!\n");
        exit(-1);
    }
    char original_buffer[BUFFER_SIZE];
    char snapshot_buffer[BUFFER_SIZE];
    ssize_t bytes_read_original, bytes_read_snapshot;
    do{
        bytes_read_original = read(original_folder, original_buffer, BUFFER_SIZE);
        bytes_read_snapshot = read(snapshot_folder, snapshot_buffer, BUFFER_SIZE);
        if(bytes_read_original != bytes_read_snapshot || memcmp(original_buffer, snapshot_buffer, bytes_read_original) != 0)
        {
            close(original_folder);
            close(snapshot_folder);
            return 1;//change detected
        }
    }while(bytes_read_original > 0 &&  bytes_read_snapshot > 0);
    if(bytes_read_original < 0 || bytes_read_snapshot < 0)
    {
        perror("Error reading files for comparison!\n");
        exit(EXIT_FAILURE);
    }

    close(original_folder);
    close(snapshot_folder);
    return 0;//No change detected
}

void Create_File_Snapshot(const char* original_path)
{
    char snapshot_path[MAX_PATH_LENGTH];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s_snapshot.txt", original_path);
    int original_folder = open(original_path, O_RDONLY);
    if(original_folder == -1)
    {
        perror("Error opening original file for reading!\n");
        exit(EXIT_FAILURE);
    }
    int snapshot_folder = open(snapshot_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(snapshot_folder == -1)
    {
        perror("Error, snapshot for respective file was not created!\n");
        close(original_folder);
        exit(-1);
    }
    struct stat original_stat;
    if(fstat(original_folder, &original_stat) == -1)
    {
        perror("Error, cannot get file status!\n");
        close(original_folder);
        close(snapshot_folder);
        exit(-1);
    }
    time_t current_time = time(NULL);//Get current time 
    struct tm* current_tm = localtime(&current_time);
    char current_time_str[20];//No more than 20 chars in the buffer according to the time
    strftime(current_time_str, sizeof(current_time_str), "%Y-%m-%d %H:%M:%S", current_tm);
    struct tm* last_modified_tm = localtime(&original_stat.st_mtime);
    char last_modified_str[20];
    strftime(last_modified_str, sizeof(last_modified_str), "%Y-%m-%d %H:%M:%S", last_modified_tm);
    char info_buffer[512];
    snprintf(info_buffer, sizeof(info_buffer), 
        "Timestamp: %s\n"
        "Entry: %s\n"
        "Size: %ld bytes\n"
        "Last modified: %s\n"
        "Permissions: %c%c%c%c%c%c%c%c%c\n"
        "Inode number: %ld\n",
        current_time_str,
        original_path,
        original_stat.st_size,
        last_modified_str,
        (S_ISDIR(original_stat.st_mode)) ? 'd' : '-',
        (original_stat.st_mode & S_IRUSR) ? 'r' : '-',
        (original_stat.st_mode & S_IWUSR) ? 'w' : '-',
        (original_stat.st_mode & S_IXUSR) ? 'x' : '-',
        (original_stat.st_mode & S_IRGRP) ? 'r' : '-',
        (original_stat.st_mode & S_IWGRP) ? 'w' : '-',
        (original_stat.st_mode & S_IXGRP) ? 'x' : '-',
        (original_stat.st_mode & S_IROTH) ? 'r' : '-',
        (original_stat.st_mode & S_IWOTH) ? 'w' : '-',
        (original_stat.st_mode & S_IXOTH) ? 'x' : '-',
        original_stat.st_ino);

        ssize_t bytes_written = write(snapshot_folder, info_buffer, strlen(info_buffer));
        if(bytes_written == -1)
        {
            perror("Error writing file information to snapshot file.\n");
            close(original_folder);
            close(snapshot_folder);
            exit(EXIT_FAILURE);
        }
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while((bytes_read = read(original_folder, buffer, BUFFER_SIZE)) > 0)
        {
            bytes_written = write(snapshot_folder, buffer, bytes_read);
            if(bytes_written == -1)
            {
                perror("Error, writing to snapshot file\n");
                close(original_folder);
                close(snapshot_folder);
                exit(EXIT_FAILURE);
            }
        }
        if(bytes_read == -1)
        {
            perror("Error reading from original file!\n");
            close(original_folder);
            close(snapshot_folder);
            exit(-1);
        }
        close(original_folder);
        close(snapshot_folder);
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
            if(Compare_Snapshots(fullPath))
            {
                printf("\nChanges detected in: %s\n", fullPath);
                //This is the flag that displays on the indicating changes
            }
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





