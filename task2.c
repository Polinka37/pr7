#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>

void to_lowercase_buf(char* dst, const char* src, size_t len) {
    for (size_t i = 0; i < len; ++i)
        dst[i] = tolower((unsigned char)src[i]);
}

int buffer_contains_word(const char* buf, size_t size, const char* word, int ignore_case) {
    size_t word_len = strlen(word);
    if (word_len > size) return 0;

    if (ignore_case) {
        char* lower_buf = malloc(size);
        char* lower_word = strdup(word);
        if (!lower_buf || !lower_word) return 0;

        to_lowercase_buf(lower_buf, buf, size);
        to_lowercase_buf(lower_word, word, word_len);

        for (size_t i = 0; i <= size - word_len; ++i) {
            if (memcmp(lower_buf + i, lower_word, word_len) == 0) {
                free(lower_buf);
                free(lower_word);
                return 1;
            }
        }
        free(lower_buf);
        free(lower_word);
        return 0;
    } else {
        for (size_t i = 0; i <= size - word_len; ++i) {
            if (memcmp(buf + i, word, word_len) == 0) {
                return 1;
            }
        }
        return 0;
    }
}

void mmap_search_in_file(const char* filepath, const char* word, int ignore_case) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Не удалось открыть файл: %s (%s)\n", filepath, strerror(errno));
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Ошибка fstat: %s (%s)\n", filepath, strerror(errno));
        close(fd);
        return;
    }

    if (st.st_size == 0) {
        close(fd);
        return;
    }

    char* map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "Ошибка mmap: %s (%s)\n", filepath, strerror(errno));
        close(fd);
        return;
    }

    size_t pos = 0, line_start = 0, line_num = 1;
    while (pos < st.st_size) {
        if (map[pos] == '\n' || pos == st.st_size - 1) {
            size_t line_len = pos - line_start + (map[pos] != '\n' ? 1 : 0);
            if (buffer_contains_word(map + line_start, line_len, word, ignore_case)) {
                printf("%s:%zu: %.*s\n", filepath, line_num, (int)line_len, map + line_start);
            }
            line_start = pos + 1;
            line_num++;
        }
        pos++;
    }

    munmap(map, st.st_size);
    close(fd);
}

void traverse_dir_mmap(const char* dirpath, const char* word, int ignore_case) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Ошибка открытия директории %s: %s\n", dirpath, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            fprintf(stderr, "Ошибка доступа к %s: %s\n", fullpath, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            traverse_dir_mmap(fullpath, word, ignore_case);
        } else if (S_ISREG(st.st_mode)) {
            mmap_search_in_file(fullpath, word, ignore_case);
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    const char* word = NULL;
    const char* root_dir = NULL;
    int ignore_case = 0;

    int idx = 1;
    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        ignore_case = 1;
        idx++;
    }

    if (argc <= idx) {
        fprintf(stderr, "Использование: %s [-i] <слово> [директория]\n", argv[0]);
        return 1;
    }

    word = argv[idx++];
    root_dir = (idx < argc) ? argv[idx] : "~/files";
    char resolved_dir[4096];
    if (root_dir[0] == '~') {
        const char* home = getenv("HOME");
        snprintf(resolved_dir, sizeof(resolved_dir), "%s%s", home, root_dir + 1);
        root_dir = resolved_dir;
    }

    traverse_dir_mmap(root_dir, word, ignore_case);
    return 0;
}
