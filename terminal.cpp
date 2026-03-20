#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

// Fase 1: Ejecutar el comando del sistema
void ejecutar_comando(const std::vector<std::string>& args_str) {
    if (args_str.empty()) return; // Si el usuario solo presiona Enter, no hace nada

    // Fase 5: Memoria Dinámica
    // Se reserva memoria usando 'new' [cite: 31] para un arreglo de punteros que requiere execvp
    char** args = new char*[args_str.size() + 1];

    for (size_t i = 0; i < args_str.size(); ++i) {
        // Reserva memoria para cada palabra del comando
        args[i] = new char[args_str[i].size() + 1];
        std::strcpy(args[i], args_str[i].c_str());
    }
    // execvp requiere que el último elemento del arreglo sea nulo
    args[args_str.size()] = nullptr; 

#ifdef _WIN32
    // En Windows se usa _spawnvp para ejecutar y esperar al proceso hijo.
    const char** args_win = new const char*[args_str.size() + 1];
    for (size_t i = 0; i < args_str.size(); ++i) {
        args_win[i] = args[i];
    }
    args_win[args_str.size()] = nullptr;

    int estado = _spawnvp(_P_WAIT, args_win[0], args_win);
    if (estado == -1) {
        std::cerr << "Comando no encontrado o error al ejecutar: " << std::strerror(errno) << "\n";
    }
    delete[] args_win;
#else
    // Se crea el proceso hijo con fork()
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "Error al crear el proceso (fork): " << std::strerror(errno) << "\n";
    } else if (pid == 0) {
        // En el proceso hijo, se ejecuta el comando con exec()
        if (execvp(args[0], args) == -1) {
            std::cerr << "Comando no encontrado o error al ejecutar: " << std::strerror(errno) << "\n";
        }
        _exit(EXIT_FAILURE);
    } else {
        // En el proceso padre. Espera a que el hijo termine.
        waitpid(pid, nullptr, 0);
    }
#endif

    // Fase 5: Liberar la memoria para evitar fugas (memory leaks)
    for (size_t i = 0; i < args_str.size(); ++i) {
        delete[] args[i];
    }
    delete[] args;
}

int main() {
    std::string linea;

    while (true) {
        // Mostrar el prompt
        std::cout << "MiniTerminal_SyJ> ";
        
        // Leer el comando desde el teclado
        if (!std::getline(std::cin, linea)) {
            std::cout << "\nSaliendo del Shell...\n";
            break; // Salir si hay EOF (Ctrl+D)
        }

        // Fase 2: Análisis de comandos (Parsing) 
        // stringstream para separar la línea por espacios fácilmente
        std::vector<std::string> args_str;
        std::istringstream iss(linea);
        std::string token;
        
        while (iss >> token) {
            args_str.push_back(token); // Guarda cada parte (ej. "ls", "-l", "/home")
        }

        if (!args_str.empty() && args_str[0] == "exit") {
            break; // Comando interno para salir
        }

        // Ejecutamos el comando analizado
        ejecutar_comando(args_str);
    }

    return 0;
}