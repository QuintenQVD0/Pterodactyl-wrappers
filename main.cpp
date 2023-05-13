#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include <cstdlib>


// Global variable to store the subprocess pid
pid_t subprocessPid = 0;

// Function to get the current timestamp as a string
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&currentTime);
    std::ostringstream oss;
    oss << std::put_time(localTime, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

// Function to launch the subprocess and capture its stdout
void launchSubprocess(const std::string& command, const std::string& logFile) {
    // Create a pipe to capture the subprocess stdout
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process

        // Close the read end of the pipe
        close(pipefd[0]);

        // Redirect the subprocess stdout to the write end of the pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        // Execute the command
        std::vector<char*> argv;
        std::istringstream iss(command);
        std::string arg;
        while (iss >> arg) {
            argv.push_back(strdup(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        perror("execvp");

        // Free the memory allocated by strdup()
        for (char* arg : argv) {
            free(arg);
        }

        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process

        // Store the subprocess pid
        subprocessPid = pid;

        // Close the write end of the pipe
        close(pipefd[1]);

        // Open the log file for appending
        std::ofstream logfile(logFile, std::ofstream::app);
        if (!logfile) {
            std::cerr << "Error opening log file: " << logFile << std::endl;
            exit(EXIT_FAILURE);
        }

        // Read from the pipe and write to both the console and the log file
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            // Get the current timestamp
            std::string timestamp = getCurrentTimestamp();

            // Write the timestamped output to the console
            std::cout << "[" << timestamp << "] " << std::string(buffer, bytesRead);

            // Write the timestamped output to the log file
            logfile << "[" << timestamp << "] " << std::string(buffer, bytesRead);
        }

        // Check for errors in reading from the pipe
        if (bytesRead == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Close the log file
        logfile.close();

        // Wait for the subprocess to finish
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        // Reset the subprocess pid
        subprocessPid = 0;
    } else {
        // Forking failed
        perror("fork");
        exit(EXIT_FAILURE);
    }
}

// Signal handler for SIGTERM
void sigtermHandler(int signum) {
    if (subprocessPid != 0) {
        // Send SIGTERM to the subprocess
        if (kill(subprocessPid, SIGTERM) == -1) {
            perror("kill");
            exit(EXIT_FAILURE);
        }

        // Wait for the subprocess to finish
        int status;
        if (waitpid(subprocessPid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        // Reset the subprocess pid
        subprocessPid = 0;
    }

    // Exit the wrapper
    exit(signum);
}

// Function to get the log directory path
std::string getLogDirectory() {
    const char* logDirEnv = std::getenv("WRAPPER_LOG_DIR");
    if (logDirEnv != nullptr && logDirEnv[0] != '\0') {
        return logDirEnv;
    } else {
        return "/home/container/logs";
    }
}

// Function to print credits
void printCredits(const std::string& logFile) {
    std::cout << "=== Pterodactyl Wrapper Application ===" << std::endl;
    std::cout << "Author: QuintenQVD0" << std::endl;
    std::cout << "Github: https://github.com/QuintenQVD0" << std::endl;
    std::cout << "Version: 1.2" << std::endl;
    std::cout << "Log File: " << logFile << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << std::endl;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        return 1;
    }
    
    // Generate the log file name with a timestamp
    std::string timestamp = getCurrentTimestamp();
    std::string logDir = getLogDirectory();
    std::string logFile = logDir + "/log_" + timestamp + ".log";

    // Create the log directory if it doesn't exist
    struct stat st;
    if (stat(logDir.c_str(), &st) == -1) {
        if (mkdir(logDir.c_str(), 0700) == -1) {
            perror("mkdir");
            return 1;
        }
    }

    // Print credits
    printCredits(logFile);

    // Join the command and arguments into a single string
    std::ostringstream oss;
    for (int i = 1; i < argc; ++i) {
        oss << argv[i] << ' ';
    }
    std::string command = oss.str();

    // Set up the signal handler for SIGTERM
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigtermHandler;
    if (sigaction(SIGTERM, &action, nullptr) == -1) {
        perror("sigaction");
        return 1;
    }

    // Launch the subprocess and capture its stdout
    launchSubprocess(command, logFile);

    return 0;
}
