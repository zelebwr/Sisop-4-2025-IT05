#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 31
#include <fuse.h> // for FUSE functions
#include <stdio.h> // for file I/O
#include <stdlib.h> // for memory allocation
#include <string.h> // for string operations
#include <unistd.h> // for POSIX functions
#include <dirent.h> // for directory handling 
#include <sys/stat.h> // for file status
#include <errno.h> // for error handling
#include <time.h> // for time functions
// #include <sys/time.h> // for time functions
// #include <sys/types.h> // for data types
// #include <fcntl.h> // for file control options

#define FRAGMENT_SIZE 1024
#define RELICS_DIR "relics"
#define LOG_FILE "activity.log"

void log_activity(const char *action, const char *filename) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm);

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        fprintf(log, "[%s] %s: %s\n", timestamp, action, filename);
        fclose(log);
    }
}

static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) { 
        stbuf->st_mode = S_IFDIR | 0755; 
        stbuf->st_nlink = 2; 
        return 0;
    }

    char base_path[256]; // base path for file checking
    snprintf(base_path, sizeof(base_path), "%s/%s", RELICS_DIR, path + 1); // after the "/"

    int total_size = 0; 
    for (int i = 0; ; i++) {
        char fragment_path[300]; 
        snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", base_path, i); // make the path for the fragment

        struct stat fragment_stat; 
        if (stat(fragment_path, &fragment_stat) == -1) break; // if there's no said file, break
        total_size += fragment_stat.st_size; // if there's the said file it will increase the total size of the will-be-created file
    }

    if (total_size > 0) {
        stbuf->st_mode = S_IFREG | 0444; // regular file with read-only permission
        stbuf->st_nlink = 1; // one link
        stbuf->st_size = total_size; // set the size of the file
        return 0; 
    }

    return -ENOENT; // file not found
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0); // current directory
    filler(buf, "..", NULL, 0); // parent directory

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (dir == NULL) return -errno; // if it fails to open the directory
    struct dirent *entry; // entry for the directory
    char current_file[256] = ""; // current file name

    while ((entry = readdir(dir)) != NULL) {
        char *dot = strrchr(entry->d_name, '.'); // find the dot in the file name
        if (dot && strcmp(dot+1, "000") == 0) { // if the file name has a dot and the next 3 characters are "000" = the first fragment
            *dot = '\0'; // remove the dot and the next 3 characters
            if (strcmp(current_file, entry->d_name) != 0) { // if the file name is not the same as the current file name
                filler(buf, entry->d_name, NULL, 0); // add the file name to the buffer
                strcpy(current_file, entry->d_name); // set the current file name to the new file name
            }
        }
    }
    closedir(dir); 
    return 0; 
}

static int baymax_open (const char *path, struct fuse_file_info *fi) {
    char first_fragment[256]; // first fragment name
    snprintf(first_fragment, sizeof(first_fragment), "%s/%s.000", RELICS_DIR, path + 1); // make the first fragment name
    
    if (access(first_fragment, F_OK) == -1) return -ENOENT; // if the first fragment does not exist

    log_activity("READ", path + 1);
    return 0; 
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char base_path[256]; // base path for file checking
    snprintf(base_path, sizeof(base_path), "%s/%s", RELICS_DIR, path + 1); // make the base path

    size_t bytes_read = 0; // bytes read
    int fragment_index = offset / FRAGMENT_SIZE; // fragment index
    off_t fragment_offset = offset % FRAGMENT_SIZE; // offset in fragment

    while (bytes_read < size)  {
        char fragment_path[300];
        snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", base_path, fragment_index); // make fragment path
        
        FILE *fragment = fopen(fragment_path, "rb");
        if (!fragment) break; // fragment doesn't exist

        fseek(fragment, fragment_offset, SEEK_SET); 
        size_t read_size = fread(buf + bytes_read, 1, size - bytes_read, fragment);
        bytes_read += read_size; // update bytes read
        fragment_offset = 0; // reset offset for next fragment
        fragment_index++; 
        fclose(fragment);
    }
    return bytes_read; // return the number of bytes read
}

static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,  // Fungsi untuk metadata
    .readdir = baymax_readdir,  // Fungsi untuk list direktori
    .open = baymax_open,     // Fungsi untuk buka file
    .read = baymax_read,     // Fungsi untuk baca file
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &baymax_oper, NULL);
}