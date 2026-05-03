#include <Python.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static int RunPythonEmbedded(int argc, char *argv[], const std::string &loader_path, const std::string &python_home) {
    PyPreConfig preconfig;
    PyStatus status;
    PyConfig config;

    PyPreConfig_InitIsolatedConfig(&preconfig);
    preconfig.utf8_mode = 1;
    status = Py_PreInitialize(&preconfig);
    if (PyStatus_Exception(status)) {
        return status.exitcode != 0 ? status.exitcode : 1;
    }

    PyConfig_InitIsolatedConfig(&config);

    status = PyConfig_SetBytesString(&config, &config.home, python_home.c_str());
    if (PyStatus_Exception(status)) {
        PyConfig_Clear(&config);
        return status.exitcode != 0 ? status.exitcode : 1;
    }

    status = PyConfig_SetBytesString(&config, &config.executable, loader_path.c_str());
    if (PyStatus_Exception(status)) {
        PyConfig_Clear(&config);
        return status.exitcode != 0 ? status.exitcode : 1;
    }

    status = PyConfig_SetBytesArgv(&config, argc, argv);
    if (PyStatus_Exception(status)) {
        PyConfig_Clear(&config);
        return status.exitcode != 0 ? status.exitcode : 1;
    }

    status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        return status.exitcode != 0 ? status.exitcode : 1;
    }

    const char *init_script =
        "import sys\n"
        "import os\n"
        "import site\n"
        "PYSTAND = os.environ['PYSTAND']\n"
        "PYSTAND_HOME = os.environ['PYSTAND_HOME']\n"
        "PYSTAND_RUNTIME = os.environ['PYSTAND_RUNTIME']\n"
        "PYSTAND_SCRIPT = os.environ['PYSTAND_SCRIPT']\n"
        "sys.path_origin = [n for n in sys.path]\n"
        "sys.loader_argv = [n for n in sys.argv]\n"
        "sys.PYSTAND = PYSTAND\n"
        "sys.PYSTAND_HOME = PYSTAND_HOME\n"
        "sys.PYSTAND_SCRIPT = PYSTAND_SCRIPT\n"
        "for n in ['.', 'lib', 'site-packages', 'runtime']:\n"
        "    test = os.path.abspath(os.path.join(PYSTAND_HOME, n))\n"
        "    if os.path.exists(test):\n"
        "        site.addsitedir(test)\n"
        "sys.argv = [PYSTAND_SCRIPT] + sys.argv[1:]\n"
        "text = open(PYSTAND_SCRIPT, 'rb').read()\n"
        "environ = {'__file__': PYSTAND_SCRIPT, '__name__': '__main__'}\n"
        "environ['__package__'] = None\n"
        "code = compile(text, PYSTAND_SCRIPT, 'exec')\n"
        "exec(code, environ)\n";

    int hr = PyRun_SimpleString(init_script);
    if (hr != 0 && PyErr_Occurred()) {
        PyErr_Print();
    }

    int finalize_hr = Py_FinalizeEx();
    if (finalize_hr < 0) {
        return 1;
    }
    return hr == 0 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    char exe_path[PATH_MAX];
    
#ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) != 0) {
        std::cerr << "Buffer too small; size needs to be " << size << std::endl;
        return 10;
    }
    // _NSGetExecutablePath 可能包含符号链接或相对路径，建议转换成绝对路径
    char real_path[PATH_MAX];
    if (realpath(exe_path, real_path) != nullptr) {
        strncpy(exe_path, real_path, sizeof(exe_path));
    }
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        std::cerr << "Failed to get executable path!" << std::endl;
        return 10;
    }
    exe_path[len] = '\0';
#endif

    std::string rootdir;
    std::string python_home;
    std::string entry_file;

    rootdir = std::string(exe_path);
    size_t pos = rootdir.find_last_of('/');
    if (pos == std::string::npos) {
        std::cerr << "Failed to parse executable directory!" << std::endl;
        return 11;
    }
    rootdir = rootdir.substr(0, pos);
    python_home = rootdir + "/runtime";
    entry_file = rootdir + "/_pystand_static.int";

    setenv("PYSTAND", exe_path, 1);
    setenv("PYSTAND_HOME", rootdir.c_str(), 1);
    setenv("PYSTAND_RUNTIME", python_home.c_str(), 1);
    setenv("PYSTAND_SCRIPT", entry_file.c_str(), 1);
    setenv("PYTHONUTF8", "1", 1);
    setenv("PYTHONCOERCECLOCALE", "1", 1);

    if (access(python_home.c_str(), X_OK) != 0) {
        std::cerr << "runtime not found or not accessible!" << std::endl;
        return 2;
    }
    if (access(entry_file.c_str(), R_OK) != 0) {
        std::cerr << "runtime/_pystand_static.int not found or not readable!" << std::endl;
        return 3;
    }

    return RunPythonEmbedded(argc, argv, exe_path, python_home);
}
