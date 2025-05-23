#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

static const char *source_dir = "/home/zele/college/programming/sms2/sisop/module4/revisi/Sisop-4-2025-IT05/soal_4/chiho";

static void get_real_path(char real_path[PATH_MAX], const char *path) {
    char temp_path[PATH_MAX];
    strcpy(temp_path, path); 

    if (strncmp(path, "/7sref/", 7) == 0) {
        char area[100];
        char filename[100];
        // format: /[area]_[filename] [cite: 77]
        sscanf(path + 7, "%[^_]_%s", area, filename);
        sprintf(temp_path, "/%s/%s", area, filename);
    }
    
    if (strncmp(temp_path, "/starter/", 9) == 0) {
        sprintf(real_path, "%s%s.mai", source_dir, temp_path);
    } else {
        sprintf(real_path, "%s%s", source_dir, temp_path);
    }
}
static void rot13(char *str) {
    if (str == NULL) return; // check for null pointer
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = 'a' + (str[i] - 'a' + 13) % 26; // rotate lowercase letters
        } else if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = 'A' + (str[i] - 'A' + 13) % 26; // rotate uppercase letters
        }
    }
}

static void metro(char *str, int size, int offset) {
    if (str == NULL) return; // check for null pointer
    for (int i = 0; i < size; i++) {
        str[i] = str[i] + (offset * (i % 256)); // apply metro transformation
    }
}

static int maimai_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = lstat(real_path, stbuf); // get file attributes
    if (res == -1) return -errno; // if it fails, return error

    return 0;
}

static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_fill_dir_flags flags) {
    (void) offset; // unused parameter
    (void) fi; // unused parameter
    (void) flags; // unused parameter

    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    DIR *dir = opendir(real_path); // open the directory
    if (dir == NULL) return -errno; // if it fails, return error

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) { // read entries
        if (strncmp(path, "/starter", 8) == 0) {
            char *mai_ext = strstr(entry->d_name, ".mai");
            if (mai_ext && strcmp(mai_ext, ".mai") == 0) {
                *mai_ext = '\0'; // remove the .mai extension
            }
        }
        if (filler(buf, entry->d_name, NULL, 0, 0)) break; // fill the buffer with entry names
    }

    closedir(dir); // close the directory
    return 0;
}

static int maimai_open(const char *path, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = open(real_path, fi->flags); // open the file
    if (res == -1) return -errno; // if it fails, return error

    fi->fh = res; // set the file descriptor
    return 0; // return success
}

static int maimai_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int fd = fi->fh; // get the file descriptor
    lseek(fd, 0, SEEK_SET); // seek to the offset  

    struct stat st;
    fstat(fd, &st); // get file attributes  

    size_t file_size = st.st_size; // get the file size
    char *file_content = malloc(file_size + 1); // allocate memory for file content

    if (read(fd, file_content, file_size) == -1) {
        free(file_content); // free the memory if read fails
        return -errno; // return error  
    }

    file_content[file_size] = '\0'; //

    if (strncmp(path, "/dragon/", 8) == 0 || (strncmp(path, "/7sref/dragon_", 14) == 0)) {
        rot13(file_content); // apply ROT13 transformation
    } else if (strncmp(path, "/metro/", 7) == 0 || (strncmp(path, "/7sref/metro_", 13) == 0)) {
        metro(file_content, file_size, -1); // apply metro transformation
    } else if (strncmp(path, "/heaven/", 8) == 0 || (strncmp(path, "/7sref/heaven_", 14) == 0)) {
        char cmd[PATH_MAX*3];
        sprintf(cmd, "openssl enc -d -aes-256-cbc -in %s -k 1234 -iv 1234", real_path);
        FILE *pipe = popen(cmd, "r");
        file_size = fread(file_content, 1, file_size, pipe); // read the decompressed content
        pclose(pipe); // close the pipe
    } else if (strncmp(path, "/youth/", 7) == 0 || (strncmp(path, "/7sref/youth_", 13) == 0)) {
        char cmd[PATH_MAX*2];
        sprintf(cmd, "gunzip -c %s", real_path);

        FILE *pipe = popen(cmd, "r");
        char* decompressed_content = malloc(st.st_size * 10); 
        file_size = fread(decompressed_content, 1, st.st_size * 10, pipe); // read the decompressed content
        free(file_content); // free the original file content

        file_content = decompressed_content; // update the file content
        pclose(pipe); // close the pipe
    }

    if (offset < file_size) {
        if (offset + size > file_size) {
            size = file_size - offset; // adjust size if it exceeds file size
        }
        memcpy(buf, file_content + offset, size); // copy the content to the buffer
    } else {
        size = 0; // set size to 0 if offset exceeds file size
    }

    free(file_content); // free the memory
    return size; // return the number of bytes read
}

static int maimai_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    char *transformed_bufer = malloc(size); // allocate memory for transformed buffer
    memcpy(transformed_bufer, buf, size); // copy the buffer content

    if (strncmp(path, "/dragon/", 8) == 0 || (strncmp(path, "/7sref/dragon_", 14) == 0)) {
        rot13(transformed_bufer); // apply ROT13 transformation
    } else if (strncmp(path, "/metro/", 7) == 0 || (strncmp(path, "/7sref/metro_", 13) == 0)) {
        metro(transformed_bufer, size, 1); // apply metro transformation
    } else if (strncmp(path, "/youth/", 7) == 0 || (strncmp(path, "/7sref/youth_", 13) == 0)) {
        char temp_file_uncompressed[] = "/tmp/maimai_youth_uncomp_XXXXXXX"; 
        int temp_fd = mkstemp(temp_file_uncompressed); // create a temporary file
        if (temp_fd == -1) {
            free(transformed_bufer); // free the memory if it fails
            return -errno; // return error
        }

        if (write(temp_fd, transformed_bufer, size) != (ssize_t)size) {
            close (temp_fd); 
            unlink (temp_file_uncompressed); 
            free(transformed_bufer); 
            return -EIO; 
        }
        close(temp_fd); 

        char cmd[PATH_MAX*3]; 
        snprintf(cmd, "gzip -c \"%s\" > \"%s\"", temp_file_uncompressed, real_path);

        int ret_system = system(cmd); 
        unlink(temp_file_uncompressed); 
        free(transformed_bufer);

        if (ret_system != 0) return -EIO; 
        return size;
    } else if (strncmp(path, "/heaven/", 8) == 0 || (strncmp(path, "/7sref/heaven_", 14) == 0)) {
        char temp_file[] = "/tmp/maimai_temp_XXXXXXX";
        int temp_fd = mkstemp(temp_file); // create a temporary file
        
        write (temp_fd, buf, size); // write the buffer to the temporary file
        
        char cmd[PATH_MAX*3];
        sprintf(cmd, "openssl enc -aes-256-cbc -in %s -out %s -k 1234 -iv 1234", temp_file, real_path);
        system(cmd); // encrypt the file
        close(temp_fd); // close the temporary file
        unlink(temp_file); // delete the temporary file
        return size; // return the number of bytes written
    }

    int res = pwrite(fi->fh, transformed_bufer, size, offset); // write the buffer to the file
    if (res == -1) return -errno; // if it fails, return
    return res; // return the number of bytes written
}

static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path
    
    int res = open(real_path, fi->flags, mode); // open the filez
    if (res == -1) return -errno; // if it fails, return error

    fi->fh = res; // set the file descriptor
    return 0; // return success
}

static int maimai_unlink (const char *path) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = unlink(real_path); // delete the file
    if (res == -1) return -errno; // if it fails, return error
    return 0; // return success
}

static int maimai_mkdir(const char *path, mode_t mode) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = mkdir(real_path, mode); // create the directory
    if (res == -1) return -errno; // if it fails, return error  
    return 0; // return success
}

static struct fuse_operations maimai_oper = {
    .getattr = maimai_getattr,
    .readdir = maimai_readdir,
    .open = maimai_open,
    .read = maimai_read,
    .write = maimai_write,
    .create = maimai_create,
    .unlink = maimai_unlink,
    .mkdir = maimai_mkdir,
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]); // print usage message
        return 1; // return error
    }

    if (strstr(source_dir, "/home/zele/college/programming/sms2/sisop/module4/revisi/Sisop-4-2025-IT05/soal_4/chiho")) {
        fprintf(stderr, "ERROR: Please edit the source_dir\n");
        return 1; // return error   
    }

    char *fuse_argv[2]; 
    fuse_argv[0] = argv[0]; // set the first argument
    fuse_argv[1] = argv[1]; // set the second argument

    return fuse_main(2, fuse_argv, &maimai_oper, NULL); // start the FUSE main loop
}