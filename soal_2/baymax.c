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
#include <ctype.h> // for character handling
#include <limits.h> // for path limits
#include <math.h> // for file math functions
// #include <sys/time.h> // for time functions
// #include <sys/types.h> // for data types
// #include <fcntl.h> // for file control options

#define FRAGMENT_SIZE 1024
#define RELICS_DIR "./relics"
#define LOG_FILE "./activity.log"

struct file_handle {
    int is_new; // flag to check if the file is new
    size_t file_size; // size of the file
    off_t total_read; // total bytes read
    int written; // flag to check if the file is written
    char written_fragments[4096]; // list of written fragments
};

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

static void get_base_path(const char *path, char *base) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        strcpy(base, last_slash + 1);
    } else {
        strcpy(base, path);
    }
}

static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat)); // clear stat struct memory
    
    // if it's a root directory
    if (strcmp(path, "/") == 0) { 
        stbuf->st_mode = S_IFDIR | 0755; // set as directory with read, write, execute permissions
        stbuf->st_nlink = 2; // links to "." and ".."
        return 0; 
    }

    char base_path[PATH_MAX]; // base path for the file
    get_base_path(path, base_path); // get the base path from the full path

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    off_t total_size = 0; // total size of the file
    struct dirent *entry; // entry struct for the directory
    int file_exists_as_fragments = 0; // flag to check if the file exists as fragments

    while ((entry = readdir(dir))) {
        if(strncmp(entry->d_name, base_path, strlen(base_path)) == 0) {
            char *dot = strrchr(entry->d_name, '.'); // find the last dot in the name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                file_exists_as_fragments = 1; // set the flag to true, meaning the file exists as fragments or exists beforehand
                char full_path[PATH_MAX]; // full path for the fragment
                snprintf(full_path, sizeof(full_path), "%s/%s", RELICS_DIR, entry->d_name); // make the full path
                struct stat fragment_stat;
                if (stat(full_path, &fragment_stat) == 0) {
                    total_size += fragment_stat.st_size;
                }
            }
        }
    }
    closedir(dir); // close the directory

    if (file_exists_as_fragments) {
        stbuf->st_mode = S_IFREG | 0666; // regular file with read-only permission
        stbuf->st_size = total_size; // set the size of the file
        stbuf->st_nlink = 1; // one link to the file
        return 0; 
    }

    return -ENOENT; // file not found
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset; // unused parameter
    (void) fi; // unused parameter

    if (strcmp(path, "/") != 0) return -ENOENT; // if the path is not root directory, meaning "No such file or directory" error

    filler(buf, ".", NULL, 0); // current directory
    filler(buf, "..", NULL, 0); // parent directory

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    char unique_files[256][256] = {0}; // array to store unique file names
    int unique_count = 0; // count of unique files

    
    struct dirent *entry; // entry for the directory
    while ((entry = readdir(dir))) {
        char *name = entry->d_name; // get the name of the entry
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue; // skip current and parent directory

        char *dot = strrchr(name, '.'); // find the dot in the file name
        if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
            char base_name[256];
            strncpy(base_name, name, dot - name);
            base_name[dot - name] = '\0'; // null-terminate the base name

            int is_duplicate = 0; // flag for duplicate check
            for (int i = 0; i < unique_count; i++) {
                if (strcmp(unique_files[i], base_name) == 0) { // if the file name is already in the list
                    is_duplicate = 1; // set duplicate flag
                    break;
                }
            }

            if (!is_duplicate && unique_count < 256) { // if it's a unique file
                strcpy(unique_files[unique_count++], base_name); // add to the list
                filler(buf, base_name, NULL, 0); // add to the buffer
            }
        }
    }
    closedir(dir);
    return 0; 
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct file_handle *fh = (struct file_handle *)(uintptr_t)fi->fh; // get the file handle
    char base_path[PATH_MAX]; // base path for the file
    get_base_path(path, base_path); // get the base path from the full path

    char *fragments[1000];
    int fragment_count = 0; // count of fragments

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    struct dirent *entry; // entry for the directory
    while((entry = readdir(dir)) && fragment_count < 1000) {
        if (strncmp(entry->d_name, base_path, strlen(base_path)) == 0) { // if the name starts with the base path
            char *dot = strrchr(entry->d_name, '.'); // find the dot in the file name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                fragments[fragment_count++] = strdup(entry->d_name); // add to the fragments list
            }
        }
    }

    closedir(dir); // close the directory

    for (int i = 0; i < fragment_count; i++) {
        for (int j = i + 1; j < fragment_count; j++) {
            if (atoi(strrchr(fragments[i], '.') + 1) > atoi(strrchr(fragments[j], '.') + 1)) {
                char *temp = fragments[i];
                fragments[i] = fragments[j];
                fragments[j] = temp;
            }
        }
    }
    
    size_t bytes_read = 0; 
    off_t current_offset = 0; 

    for (int i = 0; i < fragment_count && bytes_read < size; i++) {
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s", RELICS_DIR, fragments[i]); // make the fragment path

        FILE *f = fopen(fragment_path, "rb");
        if (!f) {
            free(fragments[i]); // free the memory
            continue; // if it fails to open the fragment, skip
        }

        fseek(f, 0, SEEK_END); // go to the end of the file
        off_t fragment_size = ftell(f); // get the size of the fragment
        rewind(f); // rewind to the beginning of the file

        if (offset < current_offset + fragment_size) { // if the fragment overlaps with the offset
            off_t fragment_offset = (offset > current_offset) ? (offset - current_offset) : 0;
            fseek(f, fragment_offset, SEEK_SET); // calculate the offset in the fragment

            size_t read_size = fmin(size - bytes_read, fragment_size - fragment_offset); // calculate the size to read
            size_t result = fread(buf + bytes_read, 1, read_size, f); // read the data from the fragment
            if (result > 0) {
                bytes_read += result; // update the bytes read
            }
        }

        current_offset += fragment_size; // update the current offset
        fclose(f); // close the fragment
        free(fragments[i]); // free the memory
    }

    fh->total_read += bytes_read; // update the total read size
    return bytes_read; // return the number of bytes read
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char base_path[PATH_MAX]; // base path for the file
    get_base_path(path, base_path); // get the base path from the full path

    struct file_handle *fh = (struct file_handle *)(uintptr_t)fi->fh; // get the file handle
    fh->written = 1; // set the written flag
    
    int start_fragment_num = offset / FRAGMENT_SIZE; // calculate the starting fragment number
    off_t offset_first_fragment = offset % FRAGMENT_SIZE; // calculate the offset in the first fragment
    size_t remaining_size = size; // remaining size to write
    size_t written_so_far = 0; // bytes written so far
    int current_fragment_num = start_fragment_num; 

    while (remaining_size > 0) {
        char fragment_path[PATH_MAX];
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s.%03d", RELICS_DIR, base_path, current_fragment_num); // make the fragment path

        char fragment_basename[256];
        snprintf(fragment_basename, sizeof(fragment_basename), "%s.%03d", base_path, current_fragment_num); // make the fragment basename
        if (strstr(fh->written_fragments, fragment_basename) == NULL) { // if the fragment is not already in the list
            if (strlen(fh->written_fragments) > 0) {
                strcat(fh->written_fragments, ", "); // add a comma if it's not the first fragment
            }
            strcat(fh->written_fragments, fragment_basename); // add the fragment basename to the list
        }

        FILE *f = fopen(fragment_path, "r+b"); // open the fragment if file exists
        if (!f) {
            f = fopen(fragment_path, "wb"); // create the fragment if it doesn't exist
        }
        if (!f) return -errno; // if it fails to create the fragment, return error

        off_t current_offset = (current_fragment_num == start_fragment_num) ? offset_first_fragment : 0; // set the offset in the fragment
        fseek(f, current_offset, SEEK_SET); // set the offset in the fragment

        size_t write_size = fmin(remaining_size, FRAGMENT_SIZE - current_offset); // calculate the size to write
        size_t bytes_written = fwrite(buf + written_so_far, 1, write_size, f); // write the data to the fragment
        fclose(f); // close the fragment

        if (bytes_written < write_size) return -EIO; // if it fails to write, return error

        written_so_far += bytes_written; // update the bytes written
        remaining_size -= bytes_written; // update the remaining size
        current_fragment_num++; // increment the fragment number
    }
    
    return size;
}

static int baymax_unlink(const char *path) {
    char base_path[PATH_MAX];
    get_base_path(path, base_path); // get the base path from the full path
    
    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    int deleted = 0; // flag for deletion
    int min_frag = INT_MAX; 
    int max_frag = INT_MIN;

    struct dirent *entry; 

    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, base_path, strlen(base_path)) == 0) { // if the name starts with the base path
            char *dot = strrchr(entry->d_name, '.'); // find the dot in the file name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                char fragment_path[PATH_MAX];
                snprintf(fragment_path, sizeof(fragment_path), "%s/%s", RELICS_DIR, entry->d_name); // make the fragment path
                if (remove(fragment_path) == 0) {
                    int frag_num = atoi(dot + 1); // get the fragment number
                    if (frag_num < min_frag) min_frag = frag_num; // update the minimum fragment number
                    if (frag_num > max_frag) max_frag = frag_num; // update the maximum fragment number
                    deleted++; // increment the deleted count
                }
            }
        }
    }
    closedir(dir); // close the directory

    if (deleted > 0) {
        char log_message[512];
        snprintf(log_message, sizeof(log_message), "%s.%03d - %s.%03d", base_path, min_frag, base_path, max_frag); // make the log message
        log_activity("DELETE", log_message); // log the delete activity
        return 0; // return success
    }
    
    return -ENOENT; // file not found
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode; // unused parameter
    
    char base[PATH_MAX]; // base path for the file
    get_base_path(path, base); // get the base path from the full path

    char existing_check_path[PATH_MAX];
    snprintf(existing_check_path, sizeof(existing_check_path), "%s/%s.000", RELICS_DIR, base); // make the path for the existing check
    if (access(existing_check_path, F_OK) == 0) { // check if the file already exists
        return -EEXIST; // file already exists
    }
    
    struct file_handle *fh = malloc(sizeof(struct file_handle)); // allocate memory for file handle
    if (!fh) return -ENOMEM; // if it fails to allocate memory

    fh->file_size = 0; // set the size of the file
    fh->total_read = 0; // set the total read size
    fh->written = 0; // set the written flag
    fh->written_fragments[0] = '\0'; // initialize the written fragments list for log buffer
    fi->fh = (uint64_t)(uintptr_t)fh; // set the file handle

    char fragment_path[PATH_MAX];
    snprintf(fragment_path, sizeof(fragment_path), "%s/%s.000", RELICS_DIR, base); // make the fragment path

    FILE *f = fopen(fragment_path, "wb"); // open the fragment for writing
    if (f){
        fclose(f); // close the fragment
        log_activity("CREATE", base); // log the create activity
    } else {
        free(fh); // free the memory if it fails to open
        return -errno; // return error
    }

    return 0;
}

static int baymax_release(const char *path, struct fuse_file_info *fi) {
    struct file_handle *fh = (struct file_handle *)(uintptr_t)fi->fh; // get the file handle
    char base_path[PATH_MAX]; // base path for the file
    get_base_path(path, base_path); // get the base path from the full path

    if (fh->written) { // if the file is written and has fragments
        char log_message[4200];
        snprintf(log_message, sizeof(log_message), "%s -> %s", path + 1, fh->written_fragments); // make the log message
        log_activity("WRITE", log_message); // log the release activity
    } else if (fh->total_read == fh->file_size && fh->file_size > 0) { // if the file is read and has fragments
        log_activity("COPY", base_path); // log the copy activity
    } else
        log_activity("READ", base_path); // log the read activity

    free(fh); 
    fi->fh = 0; // reset the file handle
    return 0;
}

static int baymax_open (const char *path, struct fuse_file_info *fi) {
    char base[PATH_MAX]; // base path for the file
    get_base_path(path, base); // get the base path from the full path

    struct file_handle *fh = malloc(sizeof(struct file_handle)); // allocate memory for file handle
    if (!fh) return -ENOMEM; // if it fails to allocate memory

    fh->total_read = 0;
    fh->file_size = 0;
    fh->written = 0;
    fh->written_fragments[0] = '\0';
    fi->fh = (uint64_t)(uintptr_t)fh;

    struct stat st;
    for (int frag_idx = 0; ; ++frag_idx) {
        char current_fragment_path[PATH_MAX]; 
        snprintf(current_fragment_path, sizeof(current_fragment_path), "%s/%s.%03d", RELICS_DIR, base, frag_idx); // make the path for the fragment
        if (stat(current_fragment_path, &st) == 0) { // check if the fragment exists
            fh->file_size += st.st_size; // update the file size
        } else {
            break; // exit the loop if the fragment doesn't exist
        }
    }
    // if file !existed, fh->file_size = 0;
    return 0; 
}

static struct fuse_operations baymax_oper ={
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open = baymax_open,
    .read = baymax_read,
    .create = baymax_create,
    .write = baymax_write,
    .unlink = baymax_unlink,
    .release = baymax_release
};

int main(int argc, char *argv[]) {
    mkdir(RELICS_DIR, 0755); // create the relics directory if it doesn't exist
    if (access(RELICS_DIR, R_OK | W_OK | X_OK) != 0) { // check if the directory is accessible
        fprintf(stderr, "Error: Cannot access %s\n", RELICS_DIR);
        return 1;
    }
    umask(0); // set the file mode creation mask to 0
    return fuse_main(argc, argv, &baymax_oper, NULL);
}