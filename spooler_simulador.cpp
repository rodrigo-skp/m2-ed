// spooler_simulador.cpp
#include <iostream>
#include <queue>
#include <string>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fstream>

namespace fs = std::filesystem;

std::queue<fs::path> fila;
std::mutex fila_mutex;
std::condition_variable cv;
bool impressora_ocupada = false;
bool rodando = true;

// Pasta temporária para simular spool directory
const std::string pasta_spool = "./spool_temp";

// Simula a impressão de um arquivo (tempo proporcional ao tamanho)
void imprimir_arquivo(const fs::path& arquivo) {
    std::ifstream in(arquivo, std::ifstream::ate | std::ifstream::binary);
    std::size_t tamanho = in.tellg();
    in.close();

    std::cout << "\n[IMPRESSÃO] Iniciando impressão do arquivo: " << arquivo.filename() << std::endl;
    std::cout << "[INFO] Tamanho do arquivo: " << tamanho << " bytes\n";

    int tempo_ms = static_cast<int>((tamanho / 1000.0) * 1000); // 1 segundo por KB
    if (tempo_ms < 1000) tempo_ms = 1000;

    std::this_thread::sleep_for(std::chrono::milliseconds(tempo_ms));

    std::cout << "[IMPRESSÃO] Impressão concluída: " << arquivo.filename() << "\n";
    fs::remove(arquivo);
}

// Thread da impressora
void spooler() {
    while (rodando) {
        std::unique_lock<std::mutex> lock(fila_mutex);
        cv.wait(lock, [] { return !fila.empty() || !rodando; });

        if (!rodando) break;

        impressora_ocupada = true;
        fs::path proximo_arquivo = fila.front();
        fila.pop();
        lock.unlock();

        imprimir_arquivo(proximo_arquivo);

        lock.lock();
        impressora_ocupada = false;
        lock.unlock();
        cv.notify_all();
    }
}

void menu_usuario() {
    int opcao;
    do {
        std::cout << "\n------ MENU ------\n";
        std::cout << "1 - Enviar arquivo para impressão\n";
        std::cout << "2 - Visualizar fila de impressão\n";
        std::cout << "0 - Sair\n";
        std::cout << "Escolha: ";
        std::cin >> opcao;

        std::cin.ignore(); // Limpa o buffer
        if (opcao == 1) {
            std::string caminho;
            std::cout << "Digite o caminho do arquivo: ";
            std::getline(std::cin, caminho);

            fs::path arquivo(caminho);
            if (!fs::exists(arquivo)) {
                std::cout << "[ERRO] Arquivo não encontrado.\n";
                continue;
            }

            // Copia o arquivo para a pasta temporária
            fs::path destino = fs::path(pasta_spool) / arquivo.filename();
            fs::copy_file(arquivo, destino, fs::copy_options::overwrite_existing);

            std::lock_guard<std::mutex> lock(fila_mutex);
            fila.push(destino);
            std::cout << "[INFO] Arquivo adicionado à fila: " << destino.filename() << "\n";
            cv.notify_all();

        } else if (opcao == 2) {
            std::lock_guard<std::mutex> lock(fila_mutex);
            if (fila.empty()) {
                std::cout << "[INFO] A fila está vazia.\n";
            } else {
                std::cout << "[FILA] Arquivos na fila:\n";
                std::queue<fs::path> temp = fila;
                int i = 1;
                while (!temp.empty()) {
                    std::cout << i++ << ". " << temp.front().filename() << "\n";
                    temp.pop();
                }
            }
        }
    } while (opcao != 0);

    // Encerra o spooler
    rodando = false;
    cv.notify_all();
}

int main() {
    if (!fs::exists(pasta_spool)) {
        fs::create_directory(pasta_spool);
    }

    std::thread spooler_thread(spooler);
    menu_usuario();
    spooler_thread.join();

    // Limpeza final da pasta temporária
    for (const auto& entry : fs::directory_iterator(pasta_spool)) {
        fs::remove(entry);
    }
    fs::remove(pasta_spool);

    std::cout << "\n[INFO] Programa encerrado.\n";
    return 0;
}
