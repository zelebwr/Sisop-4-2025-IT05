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

static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat)); // clear stat struct memory
    
    // if it's a root directory
    if (strcmp(path, "/") == 0) { 
        stbuf->st_mode = S_IFDIR | 0755; // set as directory with read, write, execute permissions
        stbuf->st_nlink = 2; // links to "." and ".."
        return 0; 
    }

    char *base_path = strrchr(path, '/') + 1; // get the base path after the last "/"

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    off_t total_size = 0; // total size of the file
    struct dirent *entry; // entry struct for the directory

    while ((entry = readdir(dir))) {
        char *name = entry->d_name; // get the name of the entry

        if (strncmp(name, base_path, strlen(base_path)) == 0) { // if the name starts with the base path
            char *dot = strrchr(name, '.'); // find the last dot in the name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                char full_path[PATH_MAX]; // full path for the fragment
                snprintf(full_path, sizeof(full_path), "%s/%s", RELICS_DIR, name); // make the full path
                struct stat fragment_stat;
                if (stat(full_path, &fragment_stat) == 0) {
                    total_size += fragment_stat.st_size;
                }
            }
        }
    }

    closedir(dir); // close the directory

    if (total_size > 0) {
        stbuf->st_mode = S_IFREG | 0666; // regular file with read-only permission
        stbuf->st_size = total_size; // set the size of the file
        stbuf->st_nlink = 1; // one link to the file
        return 0; 
    }

    return -ENOENT; // file not found
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/") != 0) return -ENOENT; // if the path is not root directory, meaning "No such file or directory" error

    filler(buf, ".", NULL, 0); // current directory
    filler(buf, "..", NULL, 0); // parent directory

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    char *unique_files[256]; // array to store unique file names
    int unique_count = 0; // count of unique files

    
    struct dirent *entry; // entry for the directory
    while ((entry = readdir(dir))) {
        char *name = entry->d_name; // get the name of the entry

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue; // skip current and parent directory

        char *dot = strrchr(name, '.'); // find the dot in the file name
        if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
            char *base_name = strndup(name, dot - name); // get the base name before the dot

            int is_duplicate = 0; // flag for duplicate check
            for (int i = 0; i < unique_count; i++) {
                if (strcmp(unique_files[i], base_name) == 0) { // if the file name is already in the list
                    is_duplicate = 1; // set duplicate flag
                    break;
                }
            }

            if (!is_duplicate) { // if it's a unique file
                unique_files[unique_count++] = base_name; // add to the list
            } else {
                free(base_name); // free the memory if it's a duplicate
            }
        }
    }
    closedir(dir); 

    for (int i = 0; i < unique_count; i++) {
        char *base_name = unique_files[i]; // get the base name
        filler(buf, base_name, NULL, 0); // add to the buffer
        free(base_name); // free the memory
    }
    return 0; 
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct file_handle *fh = (struct file_handle *)fi->fh; // get the file handle
    if (fh->is_new) return -ENOENT; // if the file is new, meaning "No such file or directory" error

    char *base = strrchr(path, '/') + 1; // get the base path after the last "/"
    if (!base) return -ENOENT; // if the base path is NULL

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    char *fragments[100]; 
    int fragment_count = 0; // count of fragments

    struct dirent *entry; // entry for the directory
    while((entry = readdir(dir))) {
        char *name = entry->d_name; // get the name of the entry

        if (strncmp(name, base, strlen(base)) == 0) { // if the name starts with the base path
            char *dot = strrchr(name, '.'); // find the dot in the file name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                fragments[fragment_count++] = strdup(name); // add to the fragments list
            } 
        }
    }

    closedir(dir); // close the directory

    for (int i = 0; i < fragment_count; i++) {
        for (int j = i + 1; j < fragment_count; j++) {
            int num_i = atoi(strrchr(fragments[i], '.') + 1);
            int num_j = atoi(strrchr(fragments[j], '.') + 1);
            if (num_i > num_j) {
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
        fseek(f, 0, SEEK_SET); // go back to the beginning of the file

        if (current_offset + fragment_size > offset) { // if the fragment overlaps with the offset
            off_t fragment_offset = offset > current_offset ? offset - current_offset : 0; // calculate the offset in the fragment

            fseek(f, fragment_offset, SEEK_SET); // set the offset in the fragment
            size_t read_size = fmin(size - bytes_read, fragment_size - fragment_offset); // read the data from the fragment
            bytes_read += fread(buf + bytes_read, 1, read_size, f); // update the bytes read
        }

        current_offset += fragment_size; // update the current offset
        fclose(f); // close the fragment
        free(fragments[i]); // free the memory
    }

    fh->total_read += bytes_read; // update the total read size
    return bytes_read; // return the number of bytes read
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char *base = strrchr(path, '/') + 1; // get the base path after the last "/"
    if (!base) return -ENOENT; // if the base path is NULL
    
    int fragment_num = 0; 
    size_t written = 0;
    char fragment_list[1024] = {0}; // list of fragments

    while (written < size) {
        char fragment_path[PATH_MAX]; 
        snprintf(fragment_path, sizeof(fragment_path), "%s/%s.%03d", RELICS_DIR, base, fragment_num); // make the fragment path

        size_t chunk_size = (size - written) > FRAGMENT_SIZE ? FRAGMENT_SIZE : (size - written); // size of the chunk to write

        FILE *f = fopen(fragment_path, "wb"); 
        if (!f) return -errno; // if it fails to open the fragment, return error

        fwrite(buf + written, 1, chunk_size, f); // write the chunk to the fragment
        fclose(f); // close the fragment

        strcat(fragment_list, fragment_num == 0 ? "" : ", "); // add a comma if it's not the first fragment
        strcat(fragment_list, strrchr(fragment_path, '/') + 1); // add the fragment name to the list

        written += chunk_size; // update the written size
        fragment_num++; // increment the fragment number
    }
    
    char log_message[2048];
    snprintf(log_message, sizeof(log_message), "%s -> %s", base, fragment_list); // make the log message
    log_activity("WRITE", log_message); // log the write activity
    return size;
}

static int baymax_unlink(const char *path) {
    char *base = strrchr(path, '/') + 1; // get the base path after the last "/"
    if (!base) return -ENOENT; // if the base path is NULL

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    int deleted = 0; // flag for deletion
    int min_frag = INT_MAX; 
    int max_frag = INT_MIN;

    struct dirent *entry; 

    while ((entry = readdir(dir))) {
        char *name = entry->d_name; // get the name of the entry
        if (strncmp(name, base, strlen(base)) == 0) { // if the name starts with the base path
            char *dot = strrchr(name, '.'); // find the dot in the file name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                char fragment_path[PATH_MAX]; 
                snprintf(fragment_path, sizeof(fragment_path), "%s/%s", RELICS_DIR, name); // make the fragment path
                if (remove(fragment_path) == 0) {
                    int frag_num = atoi(strrchr(name, '.') + 1); // get the fragment number
                    if (frag_num < min_frag) min_frag = frag_num; // update the minimum fragment number
                    if (frag_num > max_frag) max_frag = frag_num; // update the maximum fragment number
                    deleted++; // increment the deleted count
                }
            }
        }
    }
    closedir(dir); // close the directory

    if (deleted > 0) {
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "%s.%03d-%s.%03d", base, min_frag, base, max_frag); // make the log message
        log_activity("DELETE", log_message); // log the delete activity
        return 0; // return success
    }
    
    return -ENOENT; // file not found
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    struct file_handle *fh = malloc(sizeof(struct file_handle)); // allocate memory for file handle
    fh->is_new = 1; // set the new file flag
    fh->file_size = 0; // set the size of the file
    fh->total_read = 0; // set the total read size
    fi->fh = (uint64_t)fh; // set the file handle
    fi->keep_cache = 1; // keep the cache

    char *base = strrchr(path, '/') + 1; // get the base path after the last "/"
    if (!base) return -ENOENT; // if the base path is NULL

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
    struct file_handle *fh = (struct file_handle *)fi->fh; // get the file handle

    if (fh->total_read == fh->file_size && fh->file_size > 0) { // if the file is fully read
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "%s -> %s", path + 1, path + 1); 
        log_activity("COPY", log_message); // log the release activity
    }

    free(fh); 
    return 0;
}

static int baymax_open (const char *path, struct fuse_file_info *fi) {
    char *base = strrchr(path, '/') + 1; // get the base path after the last "/"
    if (!base) return -ENOENT; // if the base path is NULL

    DIR *dir = opendir(RELICS_DIR); // open relics directory
    if (!dir) return -errno; // if it fails to open the directory

    char first_fragment[PATH_MAX]; // first fragment name
    snprintf(first_fragment, sizeof(first_fragment), "%s/%s.000", RELICS_DIR, base); // make the first fragment name

    int file_existed = (access(first_fragment, F_OK) == 0); // check if the file exists
    closedir(dir); // close the directory
    
    struct file_handle *fh = malloc(sizeof(struct file_handle)); // allocate memory for file handle
    fh->is_new = !file_existed; // set the new file flag
    fi->fh = (uint64_t)fh; // set the file handle

    struct stat st; 
    if (file_existed && stat(first_fragment, &st) == 0) { 
        fh->file_size = st.st_size; // set the size of the file
        for (int i = 1; ; i++) {
            char fragment_path[4200]; 
            snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", first_fragment, i); // make the path for the fragment
            if (stat(fragment_path, &st) == -1) break; // if there's no said file, break
            fh->file_size += st.st_size; // if there's the said file, increase the total size of the will-be-created file
        }
    } else {
        fh->file_size = 0; // set the size of the file to 0
    }

    log_activity("READ", path + 1);
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
    return fuse_main(argc, argv, &baymax_oper, NULL);
}