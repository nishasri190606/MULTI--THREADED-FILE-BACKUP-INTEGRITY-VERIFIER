Multi-Threaded File Backup & Integrity Verifier

📌 Project Overview

The Multi-Threaded File Backup & Integrity Verifier is a Linux-based application developed using C programming and POSIX threads.

This project copies files from a source directory to a backup directory using multiple threads. After copying, it verifies the integrity of each file by comparing file sizes and checksums.

🎯 Objectives

- To perform file backup efficiently using multiple threads.
- To verify the integrity of copied files.
- To reduce backup time through parallel processing.
- To maintain a detailed log of backup operations.

✨ Features

- Multi-threaded file copying.
- Automatic backup directory creation.
- File integrity verification using FNV-1a checksum.
- Timestamp-based logging.
- Thread synchronization using mutex.
- Final backup statistics.
- Buffered file reading and writing.

🛠️ Technologies Used

- Programming Language: C
- Operating System: Linux
- Threading: POSIX Threads (Pthreads)
- Synchronization: Mutex
- Compiler: GCC
- File Handling: Linux System Calls

📂 Project Structure

Multi-Threaded-File-Backup-System/
│
├── main.c
├── Makefile
├── README.md
├── LICENSE
└── .gitignore

⚙️ Requirements

- Linux Operating System
- GCC Compiler
- POSIX Threads Support

🚀 Compilation

Clone the repository:

git clone YOUR_REPOSITORY_URL

Navigate to the project directory:

cd Multi-Threaded-File-Backup-System

Compile the project:

make

▶️ Execution

Run the program using:

./backup_tool <source_directory> <backup_directory> <thread_count>

Example:

./backup_tool sample_source backup_output 4

📊 Output

The program displays:

- Source directory
- Backup directory
- Number of threads started
- Total files found
- Successfully copied files
- Failed files
- Total execution time

Detailed operation logs are stored in "backup.log".

🔍 Working Principle

1. The user provides source and backup directory paths.
2. The program scans regular files in the source directory.
3. Multiple worker threads are created.
4. Each thread copies files independently.
5. File integrity is verified using size and checksum comparison.
6. Backup results are stored in a log file.
7. Final statistics are displayed.

📌 Applications

- Data backup systems
- File management systems
- Linux system utilities
- Data integrity monitoring

⚠️ Limitations

- Copies regular files directly inside the source directory only.
- Does not recursively copy subdirectories.
- Symbolic links and special files are skipped.
- Existing files with the same names in the backup directory are overwritten.

👩‍💻 Author

Developed as a Linux Programming Project using C and POSIX Threads.

📜 License

This project is licensed under the MIT License.
