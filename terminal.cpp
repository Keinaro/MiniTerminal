#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

struct Comando {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool append_output = false;
};

void mostrar_ayuda() {
    std::cout << "Comandos internos:\n";
    std::cout << "  help / ayuda  - Muestra esta ayuda\n";
    std::cout << "  history       - Muestra historial de la sesion\n";
    std::cout << "  cd <ruta>     - Cambia de directorio\n";
    std::cout << "  exit          - Cierra la terminal\n\n";
    std::cout << "Comandos propios:\n";
    std::cout << "  jorge         - Responde 'sofia'\n";
    std::cout << "  sofia         - Responde 'jorge'\n";
    std::cout << "  url           - Responde 'Universidad Rafael Landivar'\n";
    std::cout << "  materia       - Responde 'Sistemas Operativos'\n";
    std::cout << "  equipo        - Responde 'jorge y sofia'\n\n";
    std::cout << "Operadores soportados:\n";
    std::cout << "  |   Pipe entre dos o mas comandos\n";
    std::cout << "  >   Redireccion de salida\n";
    std::cout << "  >>  Redireccion de salida en modo append\n";
    std::cout << "  <   Redireccion de entrada\n";
}

std::vector<std::string> tokenizar(const std::string& linea) {
    std::istringstream iss(linea);
    std::vector<std::string> tokens;
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool parsear_linea(const std::string& linea, std::vector<Comando>& comandos, std::string& error) {
    std::vector<std::string> tokens = tokenizar(linea);
    if (tokens.empty()) {
        return false;
    }

    Comando actual;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];

        if (t == "|") {
            if (actual.args.empty()) {
                error = "Error de sintaxis: pipe sin comando antes.";
                return false;
            }
            comandos.push_back(actual);
            actual = Comando{};
            continue;
        }

        if (t == "<") {
            if (i + 1 >= tokens.size()) {
                error = "Error de sintaxis: falta archivo para redireccion de entrada.";
                return false;
            }
            actual.input_file = tokens[++i];
            continue;
        }

        if (t == ">" || t == ">>") {
            if (i + 1 >= tokens.size()) {
                error = "Error de sintaxis: falta archivo para redireccion de salida.";
                return false;
            }
            actual.output_file = tokens[++i];
            actual.append_output = (t == ">>");
            continue;
        }

        actual.args.push_back(t);
    }

    if (actual.args.empty()) {
        error = "Error de sintaxis: comando incompleto.";
        return false;
    }

    comandos.push_back(actual);
    return true;
}

void liberar_argv(char** argv, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        delete[] argv[i];
    }
    delete[] argv;
}

std::string reconstruir_linea(const std::vector<Comando>& comandos) {
    std::ostringstream oss;

    for (size_t i = 0; i < comandos.size(); ++i) {
        const Comando& c = comandos[i];
        for (size_t j = 0; j < c.args.size(); ++j) {
            if (j > 0) oss << ' ';
            oss << c.args[j];
        }

        if (!c.input_file.empty()) {
            oss << " < " << c.input_file;
        }

        if (!c.output_file.empty()) {
            oss << (c.append_output ? " >> " : " > ") << c.output_file;
        }

        if (i + 1 < comandos.size()) {
            oss << " | ";
        }
    }

    return oss.str();
}

#ifndef _WIN32
int ejecutar_pipeline_posix(const std::vector<Comando>& comandos) {
    int prev_read_fd = -1;
    std::vector<pid_t> hijos;

    for (size_t i = 0; i < comandos.size(); ++i) {
        int pipefd[2] = {-1, -1};
        const bool tiene_siguiente = (i + 1 < comandos.size());

        if (tiene_siguiente && pipe(pipefd) == -1) {
            std::cerr << "Error al crear pipe: " << std::strerror(errno) << "\n";
            return 1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Error al crear proceso (fork): " << std::strerror(errno) << "\n";
            return 1;
        }

        if (pid == 0) {
            if (prev_read_fd != -1) {
                if (dup2(prev_read_fd, STDIN_FILENO) == -1) {
                    std::cerr << "Error en dup2 (entrada de pipe): " << std::strerror(errno) << "\n";
                    _exit(EXIT_FAILURE);
                }
            }

            if (tiene_siguiente) {
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    std::cerr << "Error en dup2 (salida de pipe): " << std::strerror(errno) << "\n";
                    _exit(EXIT_FAILURE);
                }
            }

            if (!comandos[i].input_file.empty()) {
                int in_fd = open(comandos[i].input_file.c_str(), O_RDONLY);
                if (in_fd == -1) {
                    std::cerr << "No se pudo abrir archivo de entrada '" << comandos[i].input_file
                              << "': " << std::strerror(errno) << "\n";
                    _exit(EXIT_FAILURE);
                }
                if (dup2(in_fd, STDIN_FILENO) == -1) {
                    std::cerr << "Error en dup2 (redireccion entrada): " << std::strerror(errno) << "\n";
                    close(in_fd);
                    _exit(EXIT_FAILURE);
                }
                close(in_fd);
            }

            if (!comandos[i].output_file.empty()) {
                int flags = O_WRONLY | O_CREAT | (comandos[i].append_output ? O_APPEND : O_TRUNC);
                int out_fd = open(comandos[i].output_file.c_str(), flags, 0644);
                if (out_fd == -1) {
                    std::cerr << "No se pudo abrir archivo de salida '" << comandos[i].output_file
                              << "': " << std::strerror(errno) << "\n";
                    _exit(EXIT_FAILURE);
                }
                if (dup2(out_fd, STDOUT_FILENO) == -1) {
                    std::cerr << "Error en dup2 (redireccion salida): " << std::strerror(errno) << "\n";
                    close(out_fd);
                    _exit(EXIT_FAILURE);
                }
                close(out_fd);
            }

            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);
            if (prev_read_fd != -1) close(prev_read_fd);

            size_t n = comandos[i].args.size();
            char** argv = new char*[n + 1];
            for (size_t k = 0; k < n; ++k) {
                argv[k] = new char[comandos[i].args[k].size() + 1];
                std::strcpy(argv[k], comandos[i].args[k].c_str());
            }
            argv[n] = nullptr;

            execvp(argv[0], argv);
            std::cerr << "Comando no encontrado o error al ejecutar: " << std::strerror(errno) << "\n";
            liberar_argv(argv, n);
            _exit(EXIT_FAILURE);
        }

        hijos.push_back(pid);

        if (prev_read_fd != -1) close(prev_read_fd);
        if (pipefd[1] != -1) close(pipefd[1]);
        prev_read_fd = pipefd[0];
    }

    if (prev_read_fd != -1) close(prev_read_fd);

    int exit_status = 0;
    for (pid_t h : hijos) {
        int status = 0;
        waitpid(h, &status, 0);
        if (WIFEXITED(status)) {
            exit_status = WEXITSTATUS(status);
        }
    }

    return exit_status;
}
#endif

#ifdef _WIN32
int ejecutar_linea_windows_cmd(const std::string& linea) {
    int estado = _spawnlp(_P_WAIT, "cmd", "cmd", "/c", linea.c_str(), nullptr);
    if (estado == -1) {
        std::cerr << "Error al ejecutar comando en cmd: " << std::strerror(errno) << "\n";
        return 1;
    }
    return estado;
}

int ejecutar_comando_windows(const Comando& comando) {
    if (comando.args.empty()) return 0;

    size_t n = comando.args.size();
    char** args = new char*[n + 1];
    for (size_t i = 0; i < n; ++i) {
        args[i] = new char[comando.args[i].size() + 1];
        std::strcpy(args[i], comando.args[i].c_str());
    }
    args[n] = nullptr;

    const char** args_win = new const char*[n + 1];
    for (size_t i = 0; i < n; ++i) args_win[i] = args[i];
    args_win[n] = nullptr;

    int estado = _spawnvp(_P_WAIT, args_win[0], args_win);
    if (estado == -1) {
        std::cerr << "Comando no encontrado o error al ejecutar: " << std::strerror(errno) << "\n";
    }

    delete[] args_win;
    liberar_argv(args, n);
    return (estado == -1) ? 1 : 0;
}
#endif

int ejecutar_comandos(const std::vector<Comando>& comandos) {
    if (comandos.empty()) return 0;

#ifdef _WIN32
    bool requiere_cmd = (comandos.size() > 1);
    if (!requiere_cmd) {
        const Comando& c = comandos[0];
        requiere_cmd = !c.input_file.empty() || !c.output_file.empty();
    }

    if (requiere_cmd) {
        return ejecutar_linea_windows_cmd(reconstruir_linea(comandos));
    }

    return ejecutar_comando_windows(comandos[0]);
#else
    return ejecutar_pipeline_posix(comandos);
#endif
}

int main() {
    std::string linea;
    std::vector<std::string> historial;

    while (true) {
        std::cout << "MiniTerminal_SyJ> ";

        if (!std::getline(std::cin, linea)) {
            std::cout << "\nSaliendo del Shell...\n";
            break;
        }

        if (linea.empty()) {
            continue;
        }

        historial.push_back(linea);

        std::vector<Comando> comandos;
        std::string error;
        if (!parsear_linea(linea, comandos, error)) {
            if (!error.empty()) {
                std::cerr << error << "\n";
            }
            continue;
        }

        if (comandos.size() == 1 && !comandos[0].args.empty()) {
            const std::string& cmd = comandos[0].args[0];

            if (cmd == "exit") {
                break;
            }

            if (cmd == "help" || cmd == "ayuda") {
                mostrar_ayuda();
                continue;
            }

            if (cmd == "history") {
                for (size_t i = 0; i < historial.size(); ++i) {
                    std::cout << (i + 1) << ": " << historial[i] << "\n";
                }
                continue;
            }

            if (cmd == "cd") {
                if (comandos[0].args.size() < 2) {
                    std::cerr << "cd: falta ruta\n";
                    continue;
                }
#ifdef _WIN32
                if (_chdir(comandos[0].args[1].c_str()) != 0) {
#else
                if (chdir(comandos[0].args[1].c_str()) != 0) {
#endif
                    std::cerr << "cd: " << std::strerror(errno) << "\n";
                }
                continue;
            }

            if (cmd == "jorge") {
                std::cout << "sofia\n";
                continue;
            }

            if (cmd == "sofia") {
                std::cout << "jorge\n";
                continue;
            }

            if (cmd == "url") {
                std::cout << "Universidad Rafael Landivar\n";
                continue;
            }

            if (cmd == "materia") {
                std::cout << "Sistemas Operativos\n";
                continue;
            }

            if (cmd == "equipo") {
                std::cout << "jorge y sofia\n";
                continue;
            }
        }

        ejecutar_comandos(comandos);
    }

    return 0;
}