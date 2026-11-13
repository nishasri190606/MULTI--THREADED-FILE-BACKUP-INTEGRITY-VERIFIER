# Multi-Threaded File Backup & Integrity Verifier

A Linux command-line project written in C. It copies regular files from a source directory to a backup directory using POSIX worker threads, then verifies each copied file using file-size and FNV-1a checksum comparisons.

## Features

- Accepts source directory, backup directory, and thread count.
- Creates the backup directory when it does not exist.
- Copies files concurrently using `pthread`.
- Uses mutexes to protect the shared task queue, log file, and statistics.
- Reads and writes files in 64 KB buffers.
- Verifies copied files using file size and FNV-1a checksum.
- Writes timestamped results to `backup.log`.
- Displays copied/failed counts and elapsed time.

## Requirements

- Linux operating system
- GCC compiler
- POSIX threads support

## Compile

```bash
make
```

Or compile manually:

```bash
gcc -Wall -Wextra -O2 -pthread main.c -o backup_tool
```

## Run

```bash
./backup_tool <source_directory> <backup_directory> <thread_count>
```

Example:

```bash
mkdir -p sample_source
echo "Linux multithreading project" > sample_source/one.txt
echo "Backup test file" > sample_source/two.txt

./backup_tool sample_source backup_output 4
```

The program prints a summary and appends detailed entries to `backup.log` in the current working directory.

## Project limitations

- This version copies regular files located directly inside the source directory; it does not recursively copy nested subdirectories.
- Symbolic links and special files are skipped.
- The FNV-1a checksum is used for accidental-corruption detection, not cryptographic security.
- Existing files with the same names in the backup directory are overwritten.

## Concepts demonstrated

`pthread_create()`, `pthread_join()`, `pthread_mutex_t`, `open()`, `read()`, `write()`, `stat()`, `opendir()`, `readdir()`, buffered file I/O, checksum verification, and execution-time measurement.
