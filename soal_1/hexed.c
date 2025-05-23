#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>

const char *base_dir = "/home/caca/Sisop-4-2025-IT05/soal_1/anomali";

void run_command(char *cmd[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd[0], cmd);
        perror("execvp gagal");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

void download_and_unzip() {
    struct stat st;
    if (stat(base_dir, &st) == 0 && S_ISDIR(st.st_mode)) return;

    char *wget_cmd[] = {"wget", "https://drive.usercontent.google.com/u/0/uc?id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5&export=download", "-O", "anomali.zip", NULL};
    run_command(wget_cmd);

    char *unzip_cmd[] = {"unzip", "anomali.zip", "-d", ".", NULL};
    run_command(unzip_cmd);

    unlink("anomali.zip");
}

unsigned char hex_to_byte(const char *hex) {
    unsigned char byte;
    sscanf(hex, "%2hhx", &byte);
    return byte;
}

void get_timestamp(char *date_str, size_t size1, char *time_str, size_t size2) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(date_str, size1, "%Y-%m-%d", tm_info);
    strftime(time_str, size2, "%H:%M:%S", tm_info);
}

void log_conversion(const char *txt_name, const char *png_name, const char *date, const char *time) {
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/conversion.log", base_dir);
    FILE *log = fopen(log_path, "a");
    if (log) {
        fprintf(log, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n", date, time, txt_name, png_name);
        fclose(log);
    }
}

void convert_file_to_image(const char *filename_txt) {
    FILE *input = fopen(filename_txt, "r");
    if (!input) return;

    const char *basename = strrchr(filename_txt, '/');
    basename = (basename) ? basename + 1 : filename_txt;

    char name_only[64] = {0};
    strncpy(name_only, basename, strcspn(basename, "."));

    char date[16], time[16];
    get_timestamp(date, sizeof(date), time, sizeof(time));

    char image_dir[512];
    snprintf(image_dir, sizeof(image_dir), "%s/image", base_dir);
    mkdir(image_dir, 0755);

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s_image_%s_%s.png", image_dir, name_only, date, time);
    FILE *output = fopen(output_path, "wb");
    if (!output) {
        fclose(input);
        return;
    }

    char hex[3];
    int c, count = 0;
    while ((c = fgetc(input)) != EOF) {
        if (isxdigit(c)) {
            hex[count++] = c;
            if (count == 2) {
                hex[2] = '\0';
                unsigned char byte = hex_to_byte(hex);
                fwrite(&byte, 1, 1, output);
                count = 0;
            }
        }
    }

    fclose(input);
    fclose(output);

    char log_name[128];
    snprintf(log_name, sizeof(log_name), "%s_image_%s_%s.png", name_only, date, time);
    log_conversion(basename, log_name, date, time);
}

void process_all_files() {
    for (int i = 1; i <= 7; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%d.txt", base_dir, i);
        convert_file_to_image(path);
    }
}

int is_txt_file(const char *path) {
    return strstr(path, ".txt") != NULL;
}

void get_real_path(char fpath[512], const char *path) {
    snprintf(fpath, 512, "%s%s", base_dir, path);
}

void get_image_path(char img_path[512], const char *txt_path) {
    char name_only[64] = {0};
    const char *filename = strrchr(txt_path, '/');
    filename = (filename) ? filename + 1 : txt_path;
    strncpy(name_only, filename, strcspn(filename, "."));

    char image_dir[512];
    snprintf(image_dir, sizeof(image_dir), "%s/image/", base_dir);

    DIR *dir = opendir(image_dir);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, name_only) && strstr(ent->d_name, ".png")) {
                snprintf(img_path, 512, "%s%s", image_dir, ent->d_name);
                break;
            }
        }
        closedir(dir);
    }
}

int x_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char fpath[512];
    if (is_txt_file(path)) {
        char img_path[512] = {0};
        get_image_path(img_path, path);
        if (strlen(img_path) > 0)
            return lstat(img_path, stbuf);
    }
    get_real_path(fpath, path);
    return lstat(fpath, stbuf);
}

int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
              struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char fpath[512];
    get_real_path(fpath, path);
    DIR *dp = opendir(fpath);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    return 0;
}

int x_open(const char *path, struct fuse_file_info *fi) {
    char fpath[512];
    if (is_txt_file(path)) {
        char img_path[512] = {0};
        get_image_path(img_path, path);
        if (strlen(img_path) > 0)
            snprintf(fpath, sizeof(fpath), "%s", img_path);
        else
            get_real_path(fpath, path);
    } else {
        get_real_path(fpath, path);
    }

    int fd = open(fpath, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

int x_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[512];
    if (is_txt_file(path)) {
        char img_path[512] = {0};
        get_image_path(img_path, path);
        if (strlen(img_path) > 0)
            snprintf(fpath, sizeof(fpath), "%s", img_path);
        else
            get_real_path(fpath, path);
    } else {
        get_real_path(fpath, path);
    }

    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    close(fd);
    return res;
}

struct fuse_operations x_oper = {
    .getattr = x_getattr,
    .readdir = x_readdir,
    .open    = x_open,
    .read    = x_read,
};

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        char *fuse_argv[] = { argv[0], argv[1], "-o", "allow_other", NULL };
        return fuse_main(4, fuse_argv, &x_oper, NULL);
    } else {
        download_and_unzip();
        process_all_files();
        return 0;
    }
}
