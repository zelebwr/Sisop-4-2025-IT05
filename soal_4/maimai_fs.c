#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#define BASE_DIR "/home/zele/college/programming/sms2/sisop/module4/Sisop-4-2025-IT05/soal_4/maimai_fs.c"

static int is_starter_path(const char *path) {
    return strncmp(path, "/starter/", 9) ==0 || strcmp(path, "/starter") == 0; 
}

static void get_real_path(char *real_path, const char *path) {
    snprintf(real_path, PATH_MAX, "%s%s", BASE_DIR, path); 
    if (is_starter_path(path)) {
        size_t len = strlen(path);
        if (len > 0 && path[len - 1] != '/') {
            strcat(real_path, ".mai"); // append .mai if not a directory
        }
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    char real_path[PATH_MAX];
    int res;

    memset(stbuf, 0, sizeof(struct stat)); // clear the stat structure

    if (is_starter_path(path)) {
        snprintf(real_path, PATH_MAX, "%s%s", BASE_DIR, path);
        res = lstat(real_path, stbuf); // get the file status
        if (res == 0 && S_ISDIR(stbuf->st_mode)) {
            return 0; // if it's a directory, return success
        }
        
        snprintf(real_path, PATH_MAX, "%s%s.mai", BASE_DIR, path);
        res = lstat(real_path, stbuf); // get the file status
        if (res == 0) {
            return 0; // if it's a file, return success
        }
        return -ENOENT; // file not found
    } else {
        get_real_path(real_path, path); // get the real path
        res = lstat(real_path, stbuf); // get the file status
        if (res == -1) {
            return -errno; // if it fails, return error
        }
        return 0; // return success
    }
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    DIR *dp = opendir(real_path); // open the directory
    if (!dp) {
        return -errno; // if it fails to open, return error
    }

    struct dirent *de;
    while ((de = readdir(dp))) {
        struct stat st; 
        memset(&st, 0, sizeof(st)); // clear the stat structure
        st.st_ino = de->d_ino; // inode number
        st.st_mode = de->d_type << 12; // file type

        if (is_starter_path(path)) {
            if (de->d_type == DT_REG) {
                size_t len = strlen(de->d_name);
                if (len > 4 && strcmp(de->d_name + len - 4, ".mai") == 0) {
                    char display_name[256];
                    snprintf(display_name, sizeof(display_name), "%.*s", (int)(len - 4), de->d_name); // remove .mai from the name
                    
                    if (filler(buf, display_name, &st, 0))
                    break; // add to the buffer
                } else {
                    continue; // skip if not a .mai file
                }
            } else {
                if (filler(buf, de->d_name, &st, 0)) break; // add to the buffer
            }
        } else {
            if (filler(buf, de->d_name, &st, 0))
            break; // add to the buffer
        }
    }

    closedir(dp); // close the directory
    return 0; // return success
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = creat(real_path, mode); // create the file
    if (res == -1) {
        return -errno; // if it fails, return error
    }

    fi->fh = res; // set the file handle
    return 0; // return success
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = open(real_path, fi->flags); // open the file
    if (res == -1) {
        return -errno; // if it fails, return error
    }

    fi->fh = res; // set the file handle
    return 0; // return success
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;
    int res = pread(fi->fh, buf, size, offset); // read the file
    if (res == -1){
        return -errno; // if it fails, return error
    }

    return res; // return the number of bytes read
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) path;
    int res = pwrite(fi->fh, buf, size, offset); // write to the file
    if (res == -1) {
        return -errno; // if it fails, return error
    }

    return res; // return the number of bytes
}

static int xmp_unlink(const char *path) {
    char real_path[PATH_MAX];
    get_real_path(real_path, path); // get the real path

    int res = unlink(real_path); // remove the file
    if (res == -1) {
        return -errno; // if it fails, return error
    }

    return 0; // return success
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .create = xmp_create,
    .open = xmp_open,
    .read = xmp_read,
    .write = xmp_write,
    .unlink = xmp_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &xmp_oper, NULL); // start the FUSE filesystem
}