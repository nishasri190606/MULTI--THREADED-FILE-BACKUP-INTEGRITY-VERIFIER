#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 65536

typedef struct {
    char **files;
    size_t count;
    size_t next;
    const char *source;
    const char *destination;
    FILE *log_file;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t log_mutex;
    pthread_mutex_t stats_mutex;
    size_t copied;
    size_t failed;
} Shared;

static void log_message(Shared *s, const char *format, ...) {
    time_t now = time(NULL);
    struct tm tm_now;
    char timestamp[32];
    localtime_r(&now, &tm_now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);

    pthread_mutex_lock(&s->log_mutex);
    fprintf(s->log_file, "[%s] ", timestamp);
    va_list args;
    va_start(args, format);
    vfprintf(s->log_file, format, args);
    va_end(args);
    fputc('\n', s->log_file);
    fflush(s->log_file);
    pthread_mutex_unlock(&s->log_mutex);
}

static uint64_t fnv1a_update(uint64_t hash, const unsigned char *data, size_t len) {
    const uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= prime;
    }
    return hash;
}

static int copy_and_verify(const char *src, const char *dst) {
    int in_fd = open(src, O_RDONLY);
    if (in_fd < 0) return -1;

    int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        close(in_fd);
        return -1;
    }

    unsigned char buffer[BUFFER_SIZE];
    uint64_t source_hash = 14695981039346656037ULL;
    int result = 0;

    for (;;) {
        ssize_t bytes = read(in_fd, buffer, sizeof(buffer));
        if (bytes == 0) break;
        if (bytes < 0) {
            if (errno == EINTR) continue;
            result = -1;
            break;
        }

        source_hash = fnv1a_update(source_hash, buffer, (size_t)bytes);
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t n = write(out_fd, buffer + written, (size_t)(bytes - written));
            if (n < 0) {
                if (errno == EINTR) continue;
                result = -1;
                break;
            }
            written += n;
        }
        if (result < 0) break;
    }

    if (fsync(out_fd) < 0) result = -1;
    if (close(in_fd) < 0) result = -1;
    if (close(out_fd) < 0) result = -1;
    if (result < 0) return -1;

    int verify_fd = open(dst, O_RDONLY);
    if (verify_fd < 0) return -1;

    uint64_t destination_hash = 14695981039346656037ULL;
    for (;;) {
        ssize_t bytes = read(verify_fd, buffer, sizeof(buffer));
        if (bytes == 0) break;
        if (bytes < 0) {
            if (errno == EINTR) continue;
            close(verify_fd);
            return -1;
        }
        destination_hash = fnv1a_update(destination_hash, buffer, (size_t)bytes);
    }
    close(verify_fd);

    struct stat src_stat, dst_stat;
    if (stat(src, &src_stat) < 0 || stat(dst, &dst_stat) < 0) return -1;

    if (src_stat.st_size != dst_stat.st_size || source_hash != destination_hash)
        return -2;

    return 0;
}

static void *worker(void *arg) {
    Shared *s = (Shared *)arg;

    for (;;) {
        pthread_mutex_lock(&s->queue_mutex);
        if (s->next >= s->count) {
            pthread_mutex_unlock(&s->queue_mutex);
            break;
        }
        char *name = s->files[s->next++];
        pthread_mutex_unlock(&s->queue_mutex);

        char src[4096], dst[4096];
        if (snprintf(src, sizeof(src), "%s/%s", s->source, name) >= (int)sizeof(src) ||
            snprintf(dst, sizeof(dst), "%s/%s", s->destination, name) >= (int)sizeof(dst)) {
            log_message(s, "FAILED: path too long for %s", name);
            pthread_mutex_lock(&s->stats_mutex);
            s->failed++;
            pthread_mutex_unlock(&s->stats_mutex);
            continue;
        }

        int status = copy_and_verify(src, dst);
        if (status == 0) {
            log_message(s, "SUCCESS: %s copied and verified", name);
            pthread_mutex_lock(&s->stats_mutex);
            s->copied++;
            pthread_mutex_unlock(&s->stats_mutex);
        } else {
            log_message(s, "FAILED: %s (copy/verification error)", name);
            pthread_mutex_lock(&s->stats_mutex);
            s->failed++;
            pthread_mutex_unlock(&s->stats_mutex);
        }
    }
    return NULL;
}

static void free_file_list(Shared *s) {
    for (size_t i = 0; i < s->count; i++) free(s->files[i]);
    free(s->files);
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_directory> <backup_directory> <thread_count>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    long thread_count = strtol(argv[3], &end, 10);
    if (!end || *end != '\0' || thread_count < 1 || thread_count > 128) {
        fprintf(stderr, "Error: thread_count must be between 1 and 128.\n");
        return EXIT_FAILURE;
    }

    struct stat source_stat;
    if (stat(argv[1], &source_stat) < 0 || !S_ISDIR(source_stat.st_mode)) {
        fprintf(stderr, "Error: source directory does not exist or is not a directory.\n");
        return EXIT_FAILURE;
    }

    DIR *dir = opendir(argv[1]);
    if (!dir) {
        perror("opendir");
        return EXIT_FAILURE;
    }

    Shared s;
    memset(&s, 0, sizeof(s));
    s.source = argv[1];
    s.destination = argv[2];

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[4096];
        if (snprintf(full_path, sizeof(full_path), "%s/%s", s.source, entry->d_name) >= (int)sizeof(full_path))
            continue;

        struct stat st;
        if (lstat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            char **new_files = realloc(s.files, (s.count + 1) * sizeof(char *));
            if (!new_files) {
                perror("realloc");
                closedir(dir);
                free_file_list(&s);
                return EXIT_FAILURE;
            }
            s.files = new_files;
            s.files[s.count] = strdup(entry->d_name);
            if (!s.files[s.count]) {
                perror("strdup");
                closedir(dir);
                free_file_list(&s);
                return EXIT_FAILURE;
            }
            s.count++;
        }
    }
    closedir(dir);

    if (mkdir(s.destination, 0755) < 0 && errno != EEXIST) {
        perror("mkdir backup directory");
        free_file_list(&s);
        return EXIT_FAILURE;
    }

    struct stat destination_stat;
    if (stat(s.destination, &destination_stat) < 0 || !S_ISDIR(destination_stat.st_mode)) {
        fprintf(stderr, "Error: backup path is not a directory.\n");
        free_file_list(&s);
        return EXIT_FAILURE;
    }

    s.log_file = fopen("backup.log", "a");
    if (!s.log_file) {
        perror("backup.log");
        free_file_list(&s);
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&s.queue_mutex, NULL);
    pthread_mutex_init(&s.log_mutex, NULL);
    pthread_mutex_init(&s.stats_mutex, NULL);

    pthread_t *threads = calloc((size_t)thread_count, sizeof(pthread_t));
    if (!threads) {
        perror("calloc");
        fclose(s.log_file);
        free_file_list(&s);
        return EXIT_FAILURE;
    }

    struct timespec start, finish;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int created = 0;
    for (long i = 0; i < thread_count; i++) {
        int rc = pthread_create(&threads[i], NULL, worker, &s);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            break;
        }
        created++;
    }

    for (int i = 0; i < created; i++) pthread_join(threads[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &finish);

    double elapsed = (finish.tv_sec - start.tv_sec) +
                     (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("\n===== BACKUP SUMMARY =====\n");
    printf("Source directory      : %s\n", s.source);
    printf("Backup directory      : %s\n", s.destination);
    printf("Threads started       : %d\n", created);
    printf("Files found           : %zu\n", s.count);
    printf("Files copied/verified : %zu\n", s.copied);
    printf("Files failed          : %zu\n", s.failed);
    printf("Time taken            : %.3f seconds\n", elapsed);
    printf("Log file              : backup.log\n");

    log_message(&s, "SUMMARY: found=%zu copied=%zu failed=%zu time=%.3f seconds",
                s.count, s.copied, s.failed, elapsed);

    free(threads);
    free_file_list(&s);
    pthread_mutex_destroy(&s.queue_mutex);
    pthread_mutex_destroy(&s.log_mutex);
    pthread_mutex_destroy(&s.stats_mutex);
    fclose(s.log_file);

    return s.failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
