#include <iostream>
#include <pty.h>
#include <termios.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char* argv[]) {
    // Check if startup arguments for FiveM server are provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <fivem_args...>" << std::endl;
        return 1;
    }

    int master_fd;
    pid_t fivem_pid = forkpty(&master_fd, nullptr, nullptr, nullptr);

    if (fivem_pid == -1) {
        std::cerr << "Failed to fork process for FiveM server." << std::endl;
        return 1;
    } else if (fivem_pid == 0) {
        // Child process - start FiveM server
        execvp(argv[1], &argv[1]);
        return 0;
    } else {
        // Parent process - enable regular input
        struct termios old_settings, new_settings;
        tcgetattr(fileno(stdin), &old_settings);
        new_settings = old_settings;
        new_settings.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(fileno(stdin), TCSANOW, &new_settings);

        // Loop to read user input and send it to FiveM server
        char input[256];
        while (true) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fileno(stdin), &fds);
            FD_SET(master_fd, &fds);

            int max_fd = std::max(fileno(stdin), master_fd);
            int ready_fds = select(max_fd + 1, &fds, nullptr, nullptr, nullptr);

            if (ready_fds == -1) {
                std::cerr << "Error in select()" << std::endl;
                break;
            }

            if (FD_ISSET(fileno(stdin), &fds)) {
                if (fgets(input, sizeof(input), stdin) == nullptr) {
                    std::cout << "EOF received. Exiting..." << std::endl;
                    break;
                }

                write(master_fd, input, strlen(input));
            }

            if (FD_ISSET(master_fd, &fds)) {
                ssize_t bytes_read = read(master_fd, input, sizeof(input));
                if (bytes_read <= 0) {
                    std::cout << "FiveM server has stopped." << std::endl;
                    break;
                }

                std::cout.write(input, bytes_read);
                std::cout.flush();
            }
        }

        // Restore tty settings
        tcsetattr(fileno(stdin), TCSANOW, &old_settings);
    }

    return 0;
}
