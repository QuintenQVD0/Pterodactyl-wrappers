#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <pty.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <cstring>
#include <cstdlib>

void printStatus(const std::string& message) {
    std::cout << "[Wrapper] " << message << std::endl;
}

int main(int argc, char** argv) {
    // Collect all the arguments for the FiveM server command
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }

    printStatus("Wrapper started");

    // Create a pseudo-terminal (pty)
    int master_fd, slave_fd;
    char slave_name[64];
    if (openpty(&master_fd, &slave_fd, slave_name, nullptr, nullptr) == -1) {
        perror("Failed to open pseudo-terminal");
        return 1;
    }

    // Save the current terminal settings
    termios tty_settings;
    tcgetattr(STDIN_FILENO, &tty_settings);

    // Set the pseudo-terminal (pty) as the standard input and output
    dup2(slave_fd, STDIN_FILENO);
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);

    // Close the slave file descriptor
    close(slave_fd);

    // Restore the original terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &tty_settings);

    // Convert vector of arguments to char* array
    std::vector<char*> args;
    for (const auto& arg : arguments) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    // Execute the FiveM server command
    execvp(args[0], args.data());

    return 0;
}
