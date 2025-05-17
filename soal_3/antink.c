#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

#define HOST_BASE "/original_file"
#define LOG_PATH "/var/log/it24.log"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void log_action(const char *event, const char *path);

char* reverse_char(const char *name) {
    if (strstr(name, "nafis") || strstr(name, "kimcun")) {
        char msg[PATH_MAX + 100];
        snprintf(msg, sizeof(msg), "Anomaly detected %s in file: /%s", 
                strstr(name, "nafis") ? "nafis" : "kimcun", name);
        log_action("ALERT", msg);

        int len = strlen(name);
        char *reversed = malloc(len + 1);
        for (int i = 0; i < len; i++)
            reversed[i] = name[len - i - 1];
        reversed[len] = '\0';
        return reversed;
    }
    return strdup(name);
}

void geser13(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (isalpha(buf[i])) {
            if (islower(buf[i])) 
                buf[i] = 'a' + (buf[i] - 'a' + 13) % 26;
            else 
                buf[i] = 'A' + (buf[i] - 'A' + 13) % 26;
        }
    }
}

static int antink_getattr(const char *path, struct stat *st) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_BASE, path);
    return lstat(full_path, st) == -1 ? -errno : 0;
}

void log_action(const char *event, const char *path) {
    FILE *log_file = fopen(LOG_PATH, "a");
    if (log_file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
        fprintf(log_file, "[%s] [%s] %s\n", time_str, event, path);
        fclose(log_file);
    }
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;
    
    DIR *dir = opendir(HOST_BASE);
    if (!dir) return -errno;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = entry->d_ino;
        st.st_mode = entry->d_type << 12;

        char *display_name = reverse_char(entry->d_name);
        if (strcmp(display_name, entry->d_name) != 0) {
            char msg[PATH_MAX + 100];
            snprintf(msg, sizeof(msg), "File %s has been reversed : %s", 
                    entry->d_name, display_name);
            log_action("REVERSE", msg);
        }

        filler(buf, display_name, &st, 0);
        free(display_name);
    }
    closedir(dir);
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;
    
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s%s", HOST_BASE, path);

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res > 0) {
        if (!strstr(full_path, "nafis") && !strstr(full_path, "kimcun")) {
            geser13(buf, res);
            char msg[PATH_MAX + 100];
            snprintf(msg, sizeof(msg), "File %s has been encrypted", path);
            log_action("ENCRYPT", msg);
        }
    }

    close(fd);
    log_action("FILE_ACCESSED", path);
    return res;
}

static struct fuse_operations antink_ops = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .read    = antink_read,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &antink_ops, NULL);
}