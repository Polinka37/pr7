#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

void to_lowercase(char* str) {
    for (; *str; ++str) *str = tolower(*str);
}

int contains_word(const char* line, const char* word, int ignore_case) {
    if (!ignore_case) return strstr(line, word) != NULL;

    char* lower_line = strdup(line);
    char* lower_word = strdup(word);
    to_lowercase(lower_line);
    to_lowercase(lower_word);

    int result = strstr(lower_line, lower_word) != NULL;
    free(lower_line);
    free(lower_word);
    return result;
}

int search_in_file(const char* filepath, const char* word, int ignore_case) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Не удалось открыть файл: %s (%s)\n", filepath, strerror(errno));
        return 0;
    }

    char* line = NULL;
    size_t len = 0;
    int lineno = 0;
    int found = 0;

    while (getline(&line, &len, file) != -1) {
        lineno++;
        if (contains_word(line, word, ignore_case)) {
            printf("%s:%d: %s", filepath, lineno, line);
            found = 1;
        }
    }

    free(line);
    fclose(file);
    return found;
}

int search_directory(const char* dirpath, const char* word, int ignore_case) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Не удалось открыть директорию: %s (%s)\n", dirpath, strerror(errno));
        return 0;
    }

    struct dirent* entry;
    int found_any = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            fprintf(stderr, "Ошибка доступа к файлу: %s (%s)\n", fullpath, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            found_any |= search_directory(fullpath, word, ignore_case);
        } else if (S_ISREG(st.st_mode)) {
            found_any |= search_in_file(fullpath, word, ignore_case);
        }
    }

    closedir(dir);
    return found_any;
}

int main(int argc, char* argv[]) {
    const char* target_word = NULL;
    const char* search_dir = NULL;
    int ignore_case = 0;

    int arg_index = 1;
    if (argc > 1 && strcmp(argv[1], "-i") == 0) {
        ignore_case = 1;
        arg_index++;
    }

    if (argc <= arg_index) {
        fprintf(stderr, "Использование: %s [-i] <слово> [директория]\n", argv[0]);
        return 1;
    }

    target_word = argv[arg_index++];
    search_dir = (arg_index < argc) ? argv[arg_index] : "~/files";

    char resolved_path[4096];
    if (strncmp(search_dir, "~", 1) == 0) {
        const char* home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "Ошибка: не удалось определить домашнюю директорию\n");
            return 1;
        }
        snprintf(resolved_path, sizeof(resolved_path), "%s%s", home, search_dir + 1);
        search_dir = resolved_path;
    }

    int found_total = search_directory(search_dir, target_word, ignore_case);
    if (!found_total) {
        printf("Слово \"%s\" не найдено ни в одном файле.\n", target_word);
    }

    return 0;
}

