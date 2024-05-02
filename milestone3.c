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
#include <sys/wait.h> // Added for wait()
#include <libgen.h>

#define MAX_PATH_LENGTH 1024
#define BUFFER_SIZE 4096
#define MAX_CHILDREN 100

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

int Copy_File(const char* source_path, const char* destination_path)
{
    int source_fd = open(source_path, O_RDONLY);
    if(source_fd == -1)
    {
        perror("Error opening the source file for reading!\n");
        return -1;
    }
    char* destination_dir = dirname(strdup(destination_path));
    if(mkdir(destination_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST)
    {
        perror("Error creating destination directory!\n");
        close(source_fd);
        return -1;
    }
    int destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (destination_fd == -1) {
        perror("Error opening destination file for writing");
        close(source_fd);
        return -1;
    }
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_written;
    while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
        bytes_written = write(destination_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error writing to destination file");
            close(source_fd);
            close(destination_fd);
            return -1;
        }
    }
    if (bytes_read == -1) {
        perror("Error reading from source file");
        close(source_fd);
        close(destination_fd);
        return -1;
    }

    close(source_fd);
    close(destination_fd);
    return 0; // return 0 to indicate success
}

void Copy_Directory(const char* source_dir, const char* destination_dir)
{
    DIR* dir = opendir(source_dir);
    if (dir == NULL) {
        perror("Error opening source directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore special directories "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char source_path[MAX_PATH_LENGTH];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);

        char destination_path[MAX_PATH_LENGTH];
        snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_dir, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Recursively copy subdirectories
            
            Copy_Directory(source_path, destination_path);
        } else {
            // Copy regular files
            Copy_File(source_path, destination_path);
        }
    }

    closedir(dir);
}

void Process_Directory_Recursive(const char* source_dir, const char* destination_dir)
{
    DIR* dir = opendir(source_dir);
    if(dir == NULL)
    {
        perror("Error opening source directory!\n");
        return;
    }
    //Create destination directory if it does not exist
    if(mkdir(destination_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
    {
        if (errno != EEXIST) {
        perror("Error creating destination directory");
        closedir(dir);
        return;
        }
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

        if (entry->d_type == DT_DIR) {
            // Recursively copy subdirectories
            Copy_Directory(source_path, destination_path);
            Process_Directory_Recursive(source_path, destination_path);
        } else {
            // Copy regular files
            Copy_File(source_path, destination_path);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        perror("Insufficient arguments. Error usage: ./program.exe -o output_dir dir1 dir2 dri3!\n");
        exit(EXIT_FAILURE);
    }
    const char* output_dir = NULL;
    for(int index = 1; index < argc; index++)
    {
        if(strcmp(argv[index], "-o") == 0 && index < argc - 1)
        {
            output_dir = argv[index + 1];
            break;
        }
    }
    if(output_dir == NULL)
    {
        perror("Output directory not specified. Usage: ./program.exe -o output_dir dir1 dir2 dir3!\n");
        exit(-1);
    }
    struct stat st;
    if(stat(output_dir, &st) == -1)
    {
        perror("Output directory does not exist!\n");
        exit(-1);
    }
    //Check if the output directory is a directory
    if(!S_ISDIR(st.st_mode))
    {
        perror("Error, specified output directory is not a directory!\n");
        exit(EXIT_FAILURE);
    }
    /*
    argv[0] -> ./program.exe
    argv[1] -> "-o"
    argv[2] -> output_dir
    argv[3] -> dir1
    argv[4] -> dir2
    argv[5] -> dir3
    */
    pid_t children[MAX_CHILDREN];
    int num_children = 0;

    for(int index = 3; index < argc; index++)
    {
        int pid = fork();//Making processes with PID of every process made
        if(pid == -1)
        {
            perror("Error, no process running/made!\n");
            exit(EXIT_FAILURE);
        }
        if(pid == 0)//Making a child process of parent process to open recursive each directory: dir1, dir2, dir3 recursive and take snapshot of every dir and put it in the output_dir after will be close
        {
            Process_Directory_Recursive(argv[index], output_dir);
            exit(-1);
        }
        else{
            children[num_children++] = pid;
        }
    }
    for(int index = 0; index < num_children; index++)
    {
        int status;
        pid_t wpid = waitpid(children[index], &status, 0);
        if(WIFEXITED(status))
        {
            printf("The process with PID: %d has ended with the code: %d\n", wpid, WEXITSTATUS(status));
        }
    }

    return EXIT_SUCCESS;
}

/*
Every process has to make a copy of a directory open recursively and make a copy of that directory with all the files,
then go to the next subdirectory and open another process to make a copy of that subdirectory with all the files until there is no more directory to open,
after put all the copies in the output_dir which is given as command line argument after this pattern:

./program.exe -o output_dir dir1 dir2 dir3
*/
