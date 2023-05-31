#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <unistd.h>
#include <termios.h>

void printStatus(const std::string& message) {
    std::cout << "[Wrapper] " << message << std::endl;
}

void readInput(std::atomic<bool>& running) {
    // Read input from the console and send it to the subprocess
    char ch;
    while (running) {
        if (std::cin.get(ch)) {
            std::cout.put(ch);
            std::cout.flush();
        }
    }
}

int main(int argc, char** argv) {
    // Collect all the arguments for the FiveM server command
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }

    printStatus("Wrapper started");

    // Get the file descriptor for the current terminal (tty)
    int tty_fd = STDIN_FILENO;

    // Save the current terminal settings
    termios tty_settings;
    tcgetattr(tty_fd, &tty_settings);

    // Set the terminal to raw mode to pass input directly
    termios raw_settings = tty_settings;
    cfmakeraw(&raw_settings);
    tcsetattr(tty_fd, TCSANOW, &raw_settings);

    printStatus("Input mode set to raw");

    // Start the input reading thread
    std::atomic<bool> running{true};
    std::thread inputThread(readInput, std::ref(running));

    // Sleep for 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    printStatus("Starting the FiveM server");

    // Restore the original terminal settings
    tcsetattr(tty_fd, TCSANOW, &tty_settings);

    // Convert vector of arguments to char* array
    std::vector<char*> args;
    for (const auto& arg : arguments) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    // Execute the FiveM server command
    execvp(args[0], args.data());

    // Stop the input reading thread
    running = false;
    inputThread.join();

    return 0;
}
