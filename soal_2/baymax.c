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
// #include <sys/time.h> // for time functions
// #include <sys/types.h> // for data types
// #include <fcntl.h> // for file control options

#define FRAGMENT_SIZE 1024
#define RELICS_DIR "./relics"
#define LOG_FILE "./activity.log"

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
            char *dot = strrchar(name, '.'); // find the last dot in the name
            if (dot && strlen(dot) == 4 && isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3])) { // if the name has a dot and the next 3 characters are digits
                // FIXME: the full_path isn't created yet, if error.
                struct stat fragment_stat;
                stat(name, &fragment_stat); // get fragment stat
                total_size += fragment_stat.st_size; // accumulate size
            }
        }
    }

    closedir(dir); // close the directory

    if (total_size > 0) {
        stbuf->st_mode = S_IFREG | 0644; // regular file with read-only permission
        stbuf->st_size = total_size; // set the size of the file
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

    char current_file[256] = ""; // current file name
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
            if (strcmp(fragments[i], fragments[j]) > 0) { // sort the fragments from smallest to largest
                char *temp = fragments[i];
                fragments[i] = fragments[j];
                fragments[j] = temp;
            }
        }
    }
    
    size_t bytes_read = 0; 
    off_t current_offset = 0; 

    for (int i = 0; i < fragment_count && bytes_read < size; i++) {
        FILE *f = fopen(fragments[i], "rb");
        fseek(f, 0, SEEK_END); // go to the end of the file
        off_t fragment_size = ftell(f); // get the size of the fragment
        fseek(f, 0, SEEK_SET); // go back to the beginning of the file

        if (current_offset + fragment_size > offset) { // if the fragment overlaps with the offset
            fseek(f, offset - current_offset, SEEK_SET); // set the offset in the fragment
            bytes_read += fread(buf + bytes_read, 1, size - bytes_read, f); // read the fragment
        }

        current_offset += fragment_size; // update the current offset
        fclose(f); // close the fragment
        free(fragments[i]); // free the memory
    } 

    return bytes_read; // return the number of bytes read
}

static struct fuse_operations baymax_oper ={
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open = baymax_open,
    .read = baymax_read,
    .create = baymax_create,
    .write = baymax_write,
    .unlink = baymax_unlink,
};

struct file_handle {
    int is_new;
    off_t total_read; // total bytes read
    off_t file_size; // size of the file
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

static int baymax_open (const char *path, struct fuse_file_info *fi) {
    char first_fragment[256]; // first fragment name
    snprintf(first_fragment, sizeof(first_fragment), "%s/%s.000", RELICS_DIR, path + 1); // make the first fragment name

    int file_existed = (access(first_fragment, F_OK) == 0);
    
    struct file_handle *fh = malloc(sizeof(struct file_handle));
    fh->is_new = !file_existed;
    fi->fh = (uint64_t)fh;

    struct stat st; 
    if (file_existed && stat(first_fragment, &st) == 0) {
        fh->file_size = st.st_size; // set the size of the file
        for (int i = 1; ; i++) {
            char fragment_path[300]; 
            snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", first_fragment, i); // make the path for the fragment
            if (stat(fragment_path, &st) == -1) break; // if there's no said file, break
            fh->file_size += st.st_size; // if there's the said file, increase the total size of the will-be-created file
        }
    } else {
        fh->file_size = 0; // set the size of the file to 0
    }

    fi->fh = (uint64_t)fh; // set the file handle  

    log_activity("READ", path + 1);
    return 0; 
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char base_path[256]; // base path for file checking
    snprintf(base_path, sizeof(base_path), "%s/%s", RELICS_DIR, path + 1); // make the base path

    size_t bytes_written = 0; // bytes written
    while (bytes_written < size) {
        int fragment_index = (offset + bytes_written) / FRAGMENT_SIZE; // fragment index
        off_t fragment_offset = (offset + bytes_written) % FRAGMENT_SIZE; // offset in fragment
        
        char fragment_path[300];
        snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", base_path, fragment_index); // make fragment path

        FILE *fragment = fopen(fragment_path, "wb+");
        if (!fragment) return -errno; // if it fails to open the fragment

        fseek(fragment, fragment_offset, SEEK_SET); // set the offset
        size_t write_size = FRAGMENT_SIZE - fragment_offset; // size to write
        if (write_size > size - bytes_written) write_size = size - bytes_written;

        fwrite(buf + bytes_written, 1, write_size, fragment);
        bytes_written += write_size; 
        fclose(fragment);
    }
    return size; 
}

static int baymax_release (const char *path, struct fuse_file_info *fi)  {
    struct file_handle *fh = (struct file_handle *)fi->fh;

    if (fh->total_read == fh->file_size && fh->file_size > 0) {
        char log_message[256];
        snprintf(log_message, sizeof(log_message), "%s -> %s", path + 1, path + 1);
        log_activity("COPY", log_message); // log the close activity
    }
    

    if (fh->is_new) {
        char base_path[256]; // base path for file checking
        snprintf(base_path, sizeof(base_path), "%s/%s", RELICS_DIR, path + 1); // make the base path

        int count = 0; // to count the number of fragments
        off_t total_size = 0; // total size of the file

        for (int i = 0; ; i++) {
            char fragment_path[300]; // fragment path
            snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", base_path, i); // make the fragment path

            struct stat st; 
            if (stat(fragment_path, &st) == -1) break; // if the fragment does not exist
            count++;
            total_size += st.st_size; // accumulate the total size
        }

        fh->file_size = total_size; // set the size of the file

        if (count > 0) {
            char log_message[1024];
            snprintf(log_message, sizeof(log_message), "%s -> ", path + 1);
            for (int i = 0; i < count; i++) {
                char temp[50]; 
                snprintf(temp, sizeof(temp), "%s.%03d", path + 1, i);
                strcat(log_message, temp);
                if (i < count - 1) strcat(log_message, ", ");
            }
            log_activity("WRITE", log_message);
        }
    }
    free(fh); 
    return 0;
}

static int baymax_unlink(const char *path) {
    char base_path[300]; 
    snprintf(base_path, sizeof(base_path), "%s/%s", RELICS_DIR, path + 1); // make the base path

    // first and last fragment index
    int first = -1, 
        last = -1, 
        i = 0;

    while (1) {
        char fragment_path[320]; 
        snprintf(fragment_path, sizeof(fragment_path), "%s.%03d", base_path, i); // make the fragment path
        if (remove(fragment_path) == 0) { // remove the fragment
            if (first == -1) first = i; // set the first fragment index
            last = i; // set the last fragment index
            i++;
        } else {
            if (errno == ENOENT) break; // if the fragment does not exist
            else return -errno; // if it fails to remove the fragment
        }
    }

    if (first != -1) {
        char log_message[256];
        if (first == last) {
            snprintf(log_message, sizeof(log_message), "%s.%03d", path + 1, first);
        } else {
            snprintf(log_message, sizeof(log_message), "%s.%03d - %s.%03d", path + 1, first, path + 1, last);
        }
        log_activity("DELETE", log_message); // log the delete activity
        return 0;
    }
    return -ENOENT; // file not found
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char base_path[256];
    snprintf(base_path, sizeof(base_path), "%s/%s.000", RELICS_DIR, path + 1);

    // Buat fragment pertama secara eksplisit
    FILE *fragment = fopen(base_path, "wb");
    if (!fragment) return -errno;
    fclose(fragment);

    return 0;
}

static int baymax_truncate(const char *path, off_t size) {
    (void)path; (void)size; // Tidak diperlukan untuk kasus ini
    return 0;
}

static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,  // function for metadata
    .readdir = baymax_readdir,  // function for list directory
    .open = baymax_open,     // function for open file
    .read = baymax_read,     // function for read file
    .write = baymax_write,   // function for write file
    .release = baymax_release, // function for close file
    .unlink = baymax_unlink, // function for delete file
    .create = baymax_create, // function for create file
    .truncate = baymax_truncate, // function for truncate file
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &baymax_oper, NULL);
}