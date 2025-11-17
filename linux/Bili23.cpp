#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>
#include <limits.h>

std::string get_executable_dir() {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        return "";
    }
    exe_path[len] = '\0';
    std::string path(exe_path);
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

int main(int argc, char* argv[]) {
    std::string rootdir = get_executable_dir();
    std::string py_exe_path = rootdir + "/runtime/python3";
    std::string entry_file = rootdir + "/_pystand_static.int";

    if (access(py_exe_path.c_str(), X_OK) != 0) {
        std::cerr << "runtime/python3 not found or not executable!" << std::endl;
        return 2;
    }
    if (access(entry_file.c_str(), R_OK) != 0) {
        std::cerr << "runtime/_pystand_static.int not found or not readable!" << std::endl;
        return 3;
    }

    std::vector<char*> exec_args;
    exec_args.push_back(const_cast<char*>(py_exe_path.c_str()));
    exec_args.push_back(const_cast<char*>(entry_file.c_str()));
    for (int i = 1; i < argc; ++i) {
        exec_args.push_back(argv[i]);
    }
    exec_args.push_back(nullptr);

    // 启动子进程运行 Python
    pid_t pid = fork();
    if (pid == 0) {
        // 子进程：执行 runtime/python3 runtime/_pystand_.int [args...]
        execvp(py_exe_path.c_str(), exec_args.data());
        std::cerr << "Error launching python3: " << strerror(errno) << std::endl;
        exit(127);
    } else if (pid > 0) {
        // 父进程：等待子进程
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    } else {
        // fork 出错
        std::cerr << "fork() failed" << std::endl;
        return 1;
    }
}
