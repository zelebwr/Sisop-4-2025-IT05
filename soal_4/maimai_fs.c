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

#define STAGE_DIR "/chiho/starter"

struct maimai_state {
    char root_path[1024]; 
};

static void get_real_path(const char *fuse_path, char *real_path) {
    struct maimai_state *state = fuse_get_context()->private_data;
    snprintf(real_path, 1024, "%s%s%s", state->root_path, STAGE_DIR, fuse_path);
    
    if (strstr(fuse_path, "/starter/") && !strchr(fuse_path + strlen("/starter/"), '/')) {
        strcat(real_path, ".mai");
    }
}

static void remove_mai_ext(char *name) {
    char *ext = strstr(name, ".mai");
    if (ext) *ext = '\0';
}

static int maimai_getattr(const char *path, struct stat *st) {
    char real_path[1024];
    get_real_path(path, real_path); // get the real path
    
    int res = lstat(real_path, st); // get the file status
    if (res == -1) return -errno;
    return 0;
}


static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;
    char real_path[1024];
    
    get_real_path(path, real_path);
    dp = opendir(real_path);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0); // current directory
    filler(buf, "..", NULL, 0); // parent directory

    while ((de = readdir(dp)) != NULL) { // read directory entries
        struct stat st;
        memset(&st, 0, sizeof(st)); // clear the stat structure
        st.st_ino = de->d_ino; // inode number
        st.st_mode = de->d_type << 12; // file type
        
        char display_name[256];
        strcpy(display_name, de->d_name);
        
        // Hapus .mai dari nama yang ditampilkan
        if (strstr(path, "/starter/")) {
            remove_mai_ext(display_name);
        }
        
        filler(buf, display_name, &st, 0);
    }
    closedir(dp);
    return 0;
}

static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char real_path[1024];
    get_real_path(path, real_path);
    
    int fd = creat(real_path, mode);
    if (fd == -1) return -errno;
    
    fi->fh = fd;
    return 0;
}   

static int maimai_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    int fd = open(fi->fh, O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

static int maimai_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    int fd = open(fi->fh, O_WRONLY);
    if (fd == -1) return -errno;
    
    int res = pwrite(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

static int maimai_unlink(const char *path) {
    char real_path[1024];
    get_real_path(path, real_path);
    
    int res = unlink(real_path);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations maimai_oper = {
    .getattr    = maimai_getattr,
    .readdir    = maimai_readdir,
    .create     = maimai_create,
    .read       = maimai_read,
    .write      = maimai_write,
    .unlink     = maimai_unlink,
};

int main(int argc, char *argv[]) {
    struct maimai_state *state = malloc(sizeof(struct maimai_state));
    strcpy(state->root_path, "/path/to/your/chiho"); // Ganti dengan path absolut
    return fuse_main(argc, argv, &maimai_oper, state);
}