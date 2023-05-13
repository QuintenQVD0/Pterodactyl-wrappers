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
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process

        // Close the read end of the pipe
        close(pipefd[0]);

        // Redirect the subprocess stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);

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

        // Read from the pipe and write to both the console and the log file
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            std::cout.write(buffer, bytesRead);
            logfile.write(buffer, bytesRead);
        }

        // Close the log file
        logfile.close();

        // Wait for the subprocess to finish
        int status;
        waitpid(pid, &status, 0);

        // Reset the subprocess pid
        subprocessPid = 0;
    } else {
        // Forking failed
        perror("fork");
    }
}

// Signal handler for SIGTERM
void sigtermHandler(int signum) {
    if (subprocessPid != 0) {
        // Send SIGTERM to the subprocess
        kill(subprocessPid, SIGTERM);

        // Wait for the subprocess to finish
        int status;
        waitpid(subprocessPid, &status, 0);

        // Reset the subprocess pid
        subprocessPid = 0;
    }

    // Exit the wrapper
    exit(signum);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        return 1;
    }

    // Generate the log file name with a timestamp
    std::string timestamp = getCurrentTimestamp();
    std::string logDir = "/home/container/logs";
    std::string logFile = logDir + "/log_" + timestamp + ".log";

    // Create the log directory if it doesn't exist
    struct stat st;
    if (stat(logDir.c_str(), &st) == -1) {
        mkdir(logDir.c_str(), 0700);
    }

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
    sigaction(SIGTERM, &action, nullptr);

    // Launch the subprocess and capture its stdout
    launchSubprocess(command, logFile);

    return 0;
}
