# Sisop-4-2025-IT05
Below are the steps on how to use the template: 
1. Copy paste the template as many as how many questions there are
2. Change the contents of the template according to your needs
3. Don't forget to delete the template and this message before submitting the official report

Below are the template:


# Soal {n, n = the n-th question}

## Sub Soal {x, x = which sub question}

### Overview
{Fill this with a small overview on what the sub question wants you to do}

### Input/&Output
![ThisIsInput/OutputImageOfAnExample.png](assets/temp.txt)

### Code Block
```c
int main() {
    printf(%s, "fill this with your code block that function for mainly the asked purpose of the sub question");
    return 0;
}
```

### Explanation
{Fill this with your explanation about the code}

### Revision
{Here if you have any revisions that you were told to do}


# Soal 1

### Code Block
```c
#define FUSE_USE_VERSION 31
```

### Explanation
> - This line specifies that we're using FUSE API version 3.1, ensuring compatibility with the right header and library functions.

### Code Block
```c
#define FUSE_USE_VERSION 31
```

### Explanation
> - This defines the base directory where the .txt files and converted .png images reside.

## Sub Soal a

### Overview
Shorekeeper retrieves a set of anomaly samples provided as a .zip file from a given URL.
The program must:

Download the zip file

Extract its contents

Automatically delete the zip file afterward
Goal: Prepare the initial dataset of .txt files for further processing.

### Input/&Output
![hexed/hexed.png](assets/soal_1/hexed.png)


### Code Block
```c
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
```

### Explanation

> - Executes a given shell command (like wget or unzip) using fork() and execvp().

### Code Block
```c
void download_and_unzip() {
    struct stat st;
    if (stat(base_dir, &st) == 0 && S_ISDIR(st.st_mode)) return;

    char *wget_cmd[] = {...};
    run_command(wget_cmd);

    char *unzip_cmd[] = {...};
    run_command(unzip_cmd);

    unlink("anomali.zip");
}
```

### Explanation

> - Checks if the data directory exists. If not, downloads and extracts the ZIP file containing .txt data.


### Sub soal b

### Overview
Upon inspection, each .txt file contains hexadecimal strings instead of readable text.
Shorekeeper realizes the files are encoded and may represent binary data such as images.
Whenever a .txt file is accessed from the mounted directory (via FUSE), its content should be automatically converted into a .png image.
Goal: Understand that each file contains encoded hex that must be transformed.
Goal: Transparently transform .txt (hex) into .png (image) on access using FUSE.

### Input & Output
![hexed/hexed.png](assets/soal_1/hexed.png)

### Code Block
```c
unsigned char hex_to_byte(const char *hex) {
    unsigned char byte;
    sscanf(hex, "%2hhx", &byte);
    return byte;
}
```

### Explanation
> - Used during the conversion of .txt files that contain hexadecimal representations.

### Code Block
```c
void process_all_files() {
    for (int i = 1; i <= 7; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%d.txt", base_dir, i);
        convert_file_to_image(path);
    }
}
```

### Explanation

> - Loops from 1.txt to 7.txt, converting each one if available.

##### FUSE Function

### Input & Output
![mnt/mnt.png](assets/soal_1/mkdirmnt.png)

### Code Block
```c
int is_txt_file(const char *path) {
    return strstr(path, ".txt") != NULL;
}
```

### Explanation

> - Helper to identify whether the current file is a .txt (eligible for conversion).

### Code Block
```c
void get_real_path(char fpath[512], const char *path) {
    snprintf(fpath, 512, "%s%s", base_dir, path);
}

```

### Explanation

> - Generates the full path of a file by appending the virtual path to the base directory.

### Code Block
```c
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
```

### Explanation

> - Given a .txt file, tries to locate the corresponding .png inside anomali/image.

### Code Block
```c
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
```

### Explanation

> - Provides file attributes for .png or .txt, depending on availability.

### Code Block
```c
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
```

### Explanation

> - Reads directory contents and fills them into the virtual filesystem.

### Code Block
```c
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
```

### Explanation

> - Opens .png if it exists instead of .txt, making the switch transparent.

### Code Block
```c
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
```

### Explanation

> - Reads the .png version of a file if available. Otherwise, reads the raw .txt.

### Code Block
```c
struct fuse_operations x_oper = {
    .getattr = x_getattr,
    .readdir = x_readdir,
    .open    = x_open,
    .read    = x_read,
};
```

### Explanation

> - Declares the functions used by the FUSE filesystem interface.

### Code Block
```c
int main(int argc, char *argv[]) {
    if (argc >= 2) {
        ...
        return fuse_main(...);
    } else {
        download_and_unzip();
        process_all_files();
        return 0;
    }
}

```

### Explanation

> - If run with an argument (mount point), the FUSE filesystem is mounted.
Otherwise, it just runs the downloader and converter.


### Sub soal c

### Overview
Each generated image must follow a specific naming format:
```[original_filename]_image_[YYYY-mm-dd]_[HH:MM:SS].png```
Goal: Ensure each image file is uniquely timestamped and traceable to its source .txt.

### Input & Output
![mnt/mnt.png](assets/soal_1/mnt.png)

### Code Block
```c
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
```

### Explanation

> - Reads hex data from .txt, decodes it, and writes to a binary .png file with a timestamped filename.

### Sub soal d

### Overview
Every successful conversion must be recorded in a conversion.log file inside the anomali directory.
The log entry format:
```[YYYY-mm-dd][HH:MM:SS]: Successfully converted hexadecimal text [filename] to [output_image_name].```

### Input & Output
![log/log.png](assets/soal_1/clog.png)

### Code Block
```c
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
```

### Explanation

> - Fills in current date and time, used in naming converted image files and logs.
> - Logs each conversion process into a file called conversion.log.

### Input & Output
![tree/tree.png](assets/soal_1/tree.png)

### Revision
> Fixed issue with FUSE mount – previously 'mnt' directory was inaccessible. Adjusted the logic to ensure proper mounting and access.


# Soal 3

## Sub Soal a

### Overview
Docker Compose is used to orchestrate the main FUSE-based file system service and its logging system.

### Code Block (.yml)
```c
# docker-compose.yml
version: '3'

services:
  antink-server:
    build: .
    volumes:
      - it24_host:/original_file    # Bind mount ke direktori host
      - antink_mount:/antink_mount  # Mount point FUSE
      - antink-logs:/var/log        # Log storage
    privileged: true
    command: /app/antink /antink_mount -o allow_other

  antink-logger:
    image: alpine
    volumes:
      - antink-logs:/var/log
    command: tail -f /var/log/*.log # Monitor semua log

volumes:
  it24_host:    # Store Original File
    driver: local
    driver_opts:
      type: none
      o: bind
      device: ./original
  antink_mount: # Mount Point
    driver: local
    driver_opts:
      type: none
      o: bind
      device: ./mount
  antink-logs:  # Store Log
    driver: local
    driver_opts:
      type: none
      o: bind
      device: ./logs
```
### Code Block (Dockerfile)
```
# Dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y fuse3 libfuse3-dev gcc wget
RUN mkdir -p /original_file /app

# Download file contoh
RUN wget -O /original_file/nafis.jpg "https://docs.google.com/uc?export=download&id=1_lEz_pV3h4uippLfOLeqO1bQ5bM8a1dy"
RUN wget -O /original_file/kimcun.jpg "https://docs.google.com/uc?export=download&id=18R58eAfJ-1xkgE57WjzFgc16w2UdFlBU"

COPY antink.c /app/
RUN gcc -D_FILE_OFFSET_BITS=64 /app/antink.c -o /app/antink `pkg-config fuse3 --cflags --libs`

CMD ["/app/antink", "/antink_mount"]
```

### Explanation
Docker Compose sets up 2 services: antink-server (FUSE) and antink-logger (log monitoring).
Volume it24_host is used for the original files, and antink_mount is the FUSE mount point.
The Dockerfile performs the following:
> - Installs FUSE3 dependencies
> - Downloads sample files to /original_file
> - Compiles the FUSE program from antink.c

## Sub Soal b

### Overview
Filenames containing certain keywords are reversed while preserving their extension.

### Code Block (.c)
```c
// Fungsi reverse filename dengan ekstensi
char* reverse_char(const char *name) {
    if(strstr(name, "nafis") || strstr(name, "kimcun")){
        char *dot = strrchr(name, '.');
        int name_len = dot ? (dot - name) : strlen(name);
        
        // Reverse nama file tanpa ekstensi
        char *reversed = malloc(strlen(name)+1);
        for(int i=0; i<name_len; i++){
            reversed[i] = name[name_len-1-i];
        }
        
        // Tambah ekstensi
        if(dot) strcat(reversed, dot);
        
        // Log alert
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "ALERT: File %s reversed to %s", name, reversed);
        log_action("ALERT", log_msg);
        
        return reversed;
    }
    return strdup(name);
}

// FUSE readdir implementation
static int antink_readdir(...) {
    while((entry = readdir(dir))) {
        char *display_name = reverse_char(entry->d_name);
        filler(buf, display_name, &st, 0);
        free(display_name);
    }
}
```
### Explanation
The reverse_char function detects keywords and reverses the filename.
It preserves the file extension (example: "nafis.txt" becomes "sifan.txt").
Logging is automatically done to alert.log.

## Sub Soal c

### Overview
Files without keywords are encrypted using ROT13, with logs recorded.

### Code Block (.c)
```c
void geser13(char *buf, size_t size) {
    for(size_t i=0; i<size; i++) {
        if(isalpha(buf[i])) {
            char base = islower(buf[i]) ? 'a' : 'A';
            buf[i] = (buf[i] - base + 13) % 26 + base;
        }
    }
}

// FUSE read implementation
static int antink_read(...) {
    if(!strstr(path, "nafis") && !strstr(path, "kimcun")) {
        geser13(buf, res);
        log_action("ENCRYPT", path);
    }
}
```
### Explanation
ROT13 encryption is applied only to files that do not contain the keyword.
Encryption activity is logged in encrypt.log.

## Sub Soal d

### Overview
Events are categorized and logged separately, then monitored by a dedicated service.

### Code Block (.c)
```c
void log_action(const char *event, const char *path) {
    const char *log_file;
    if(strcmp(event, "ALERT") == 0) log_file = "/var/log/alert.log";
    else if(strcmp(event, "ENCRYPT") == 0) log_file = "/var/log/encrypt.log";
    
    FILE *log = fopen(log_file, "a");
    fprintf(log, "[%s] %s - %s\n", timestamp, event, path);
    fclose(log);
}
```
### Explanation
Logs are separated into 3 different files based on the type of event.
The antink-logger service performs a tail operation on all log files.

## Sub Soal e

### Overview
All file interactions happen inside the container to ensure isolation and safety.

### Code Block (.yml)
```c
volumes:
  it24_host:
    driver_opts:
      o: bind
      device: ./original # File host asli tidak terpengaruh
```
### Explanation
All file operations are performed inside the container via FUSE.
Bind mount is used only for read-only access to the original files.
Changes occur only at the mount point inside the container (/antink_mount).

### Main Revision
I couldn't run my code on Parrot OS because it requires Docker, and Docker isn't fully compatible with Parrot. I tried to install it, but the system kept throwing errors due to conflicts and missing support.
