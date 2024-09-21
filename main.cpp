#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <utility>
#include <queue>
#include <random>

#define Width 800
#define Height 600
#define dw 30
#define dh 30

std::mutex matrixMutex;
std::condition_variable cv;
bool matrixReady = false;
std::vector<std::vector<int>> adjacencyMatrix;
std::vector<std::pair<int, int>> path; // Para almacenar el camino encontrado

// Función para dibujar un cuadrado de color en una posición específica
void Draw(sf::RenderWindow& window, int A, int B, sf::Color color) {
    // Eliminar esta línea, ya que invierte las coordenadas Y
    // B = Height / dh - 1 - B;
    if (A < 0 || B < 0 || A >= Width / dw || B >= Height / dh) return;

    sf::RectangleShape square(sf::Vector2f(dw, dh));
    square.setFillColor(color);
    square.setPosition(A * dw, B * dh);
    window.draw(square);
}

// Clase para manejar un "player" con una imagen PNG
class Player {
private:
    sf::Sprite sprite;
    sf::Texture texture;
    std::pair<int, int> position;
    bool _isMoving; // Nuevo flag para indicar movimiento
    std::random_device rd;
    std::mt19937 gen;
    sf::Clock moveClock;    // Nuevo temporizador
    float updateInterval;   // Intervalo para la actualización del movimiento

public:
    Player(const std::string& textureFile) : _isMoving(false), gen(rd()), updateInterval(0.2f) {  // Default updateInterval
        if (!texture.loadFromFile(textureFile)) {
            std::cerr << "Error loading texture: " << textureFile << std::endl;
        }
        sprite.setTexture(texture);
        position = {0, 0}; // Inicializar posición
    }

    // Configurar el intervalo de actualización de movimiento
    void setUpdateInterval(float interval) {
        updateInterval = interval;
    }

    void setPosition(float x, float y) {
        sprite.setPosition(x, y);
        position = {static_cast<int>(x / dw), static_cast<int>(y / dh)};
    }

    void setSize(float width, float height) {
        sf::Vector2u textureSize = texture.getSize();
        float scaleX = width / textureSize.x;
        float scaleY = height / textureSize.y;
        sprite.setScale(scaleX, scaleY);
    }

    void draw(sf::RenderWindow& window) {
        window.draw(sprite);
    }

    void startMoving() {
        if (!path.empty()) {
            _isMoving = true; // Inicia el movimiento
        }
    }

    std::pair<int, int> getPosition() const {
        return position;
    }

    void bfsMove(int targetX, int targetY, const std::vector<std::vector<int>>& matrix, const std::vector<Player*>& players) {
        if (targetX < 0 || targetX >= matrix[0].size() || targetY < 0 || targetY >= matrix.size()) {
            return;
        }
        std::queue<std::pair<int, int>> q;
        std::vector<std::vector<bool>> visited(matrix.size(), std::vector<bool>(matrix[0].size(), false));
        std::vector<std::vector<std::pair<int, int>>> parent(matrix.size(), std::vector<std::pair<int, int>>(matrix[0].size(), {-1, -1}));

        q.push(position);
        visited[position.first][position.second] = true;

        while (!q.empty()) {
            auto current = q.front();
            q.pop();

            if (current.second == targetX && current.first == targetY) {
                path.clear();
                std::pair<int, int> step = current;
                while (step != position) {
                    path.push_back(step);
                    step = parent[step.first][step.second];
                }
                std::reverse(path.begin(), path.end());
                return;
            }

            for (const auto& direction : std::vector<std::pair<int, int>>{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}) {
                int neighborX = current.second + direction.second;
                int neighborY = current.first + direction.first;

                if (neighborX >= 0 && neighborY >= 0 && neighborX < matrix[0].size() && neighborY < matrix.size() &&
                    matrix[neighborY][neighborX] != -1 && !visited[neighborY][neighborX]) {

                    // Check if the neighbor position is occupied by another player
                    bool isOccupied = false;
                    for (const Player* player : players) {
                        if (player != this && player->getPosition() == std::make_pair(neighborY, neighborX)) {
                            isOccupied = true;
                            break;
                        }
                    }

                    if (!isOccupied) {
                        visited[neighborY][neighborX] = true;
                        parent[neighborY][neighborX] = current;
                        q.push({neighborY, neighborX});
                    }
                }
            }
        }

        path.clear();
    }

    void randomMove(int targetX, int targetY, const std::vector<std::vector<int>>& matrix, const std::vector<Player*>& players) {
        path.clear();
        std::pair<int, int> current = position;

        while (current.first != targetY || current.second != targetX) {
            std::vector<std::pair<int, int>> validMoves;

            std::vector<std::pair<int, int>> directions = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            for (const auto& dir : directions) {
                int newX = current.second + dir.second;
                int newY = current.first + dir.first;

                if (newX >= 0 && newX < matrix[0].size() && newY >= 0 && newY < matrix.size() && matrix[newY][newX] != -1) {
                    // Check if the new position is occupied by another player
                    bool isOccupied = false;
                    for (const Player* player : players) {
                        if (player != this && player->getPosition() == std::make_pair(newY, newX)) {
                            isOccupied = true;
                            break;
                        }
                    }

                    if (!isOccupied) {
                        validMoves.push_back({newY, newX});
                    }
                }
            }

            if (validMoves.empty()) {
                break;
            }

            std::uniform_int_distribution<> dis(0, validMoves.size() - 1);
            auto nextMove = validMoves[dis(gen)];

            path.push_back(nextMove);
            current = nextMove;
        }
    }

    // Elegir el algoritmo y configurar la velocidad según el tipo de movimiento
    bool moveTo(int targetX, int targetY, const std::vector<std::vector<int>>& matrix, const std::vector<Player*>& players) {
        if (targetX < 0 || targetX >= matrix[0].size() || targetY < 0 || targetY >= matrix.size()) {
            std::cout << "Posición fuera de los límites." << std::endl;
            return false;
        }

        for (Player* player : players) {
            if (player != this && player->getPosition() == std::make_pair(targetY, targetX)) {
                std::cout << "La posición está ocupada por otro jugador." << std::endl;
                return false;
            }
        }

        std::uniform_int_distribution<> dis(0, 1);
        if (dis(gen) == 0) {
            bfsMove(targetX, targetY, matrix, players);
            setUpdateInterval(0.1f);
        } else {
            randomMove(targetX, targetY, matrix, players);
            setUpdateInterval(0.001f);
        }
        _isMoving = !path.empty();
        return _isMoving;
    }


    bool isMoving() const {
        return _isMoving;
    }

    void clearPath() {
        path.clear();
        _isMoving = false;
    }

    void updatePosition() {
        if (_isMoving && !path.empty()) {
            // Si ha pasado suficiente tiempo, actualizar la posición
            if (moveClock.getElapsedTime().asSeconds() >= updateInterval) {
                auto nextStep = path.front();
                path.erase(path.begin());

                // Actualiza la posición del jugador
                setPosition(nextStep.second * dw, nextStep.first * dh);

                // Reiniciar el temporizador después de mover
                moveClock.restart();

                // Verifica si hemos llegado a la última posición
                if (path.empty()) {
                    _isMoving = false;
                    position = nextStep;
                }
            }
        }
    }

    bool hasPath() const {
        return !path.empty();
    }
};

// Función para generar una matriz de adyacencia y obstáculos
void generateAdjacencyMatrix(int rows, int cols, int obstacleCount) {
    std::vector<std::vector<int>> tempMatrix(rows, std::vector<int>(cols, 0));

    // Generar posiciones para obstáculos
    std::vector<std::pair<int, int>> obstaclePositions;

    while (obstaclePositions.size() < obstacleCount) {
        int x = rand() % cols;
        int y = rand() % rows;
        if (std::find(obstaclePositions.begin(), obstaclePositions.end(), std::make_pair(x, y)) == obstaclePositions.end()) {
            obstaclePositions.emplace_back(x, y);
            tempMatrix[y][x] = -1; // Marcar posición como obstáculo
        }
    }

    {
        std::lock_guard<std::mutex> lock(matrixMutex);
        adjacencyMatrix = std::move(tempMatrix);
        matrixReady = true;
    }

    cv.notify_one();
}

int main() {
    try {
        srand(static_cast<unsigned int>(time(0)));

        sf::RenderWindow window(sf::VideoMode(Width, Height), "Ventana de Juego");

        Player player1("/home/iquick/Documents/Proyectos C/Proyecto2/ProyectoII/Assets/TanqueCeleste.png");
        Player player2("/home/iquick/Documents/Proyectos C/Proyecto2/ProyectoII/Assets/TanqueAzul.png");

        player1.setSize(dw - 2, dh - 2); // Ajustar tamaño para que quepa en la celda
        player1.setPosition(dw, dh); // Posicionar en la primera celda

        player2.setSize(dw - 2, dh - 2); // Ajustar tamaño para que quepa en la celda
        player2.setPosition(dw * 2, dh); // Posicionar en la segunda celda

        int rows = Height / dh;
        int cols = Width / dw;
        int obstacleCount = 10;
        bool player1TurnCompleted = false;
        bool player2TurnCompleted = false;
        bool player1Turn = true;
        bool moveInitiated = false;


        std::thread matrixThread(generateAdjacencyMatrix, rows, cols, obstacleCount);

        sf::Clock clock;

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();

                if (event.type == sf::Event::MouseButtonPressed) {
                    if (event.mouseButton.button == sf::Mouse::Left) {
                        int targetX = event.mouseButton.x / dw;
                        int targetY = event.mouseButton.y / dh;

                        std::vector<Player*> players = {&player1, &player2};

                        if (!moveInitiated) {
                            if (player1Turn) {
                                moveInitiated = player1.moveTo(targetX, targetY, adjacencyMatrix, {&player1, &player2});
                            } else {
                                moveInitiated = player2.moveTo(targetX, targetY, adjacencyMatrix, {&player1, &player2});
                            }
                        }
                    }
                }
            }

    {
        std::unique_lock<std::mutex> lock(matrixMutex);
        cv.wait(lock, [] { return matrixReady; });
    }

    window.clear(sf::Color::White);

    // Dibujar cuadrícula y obstáculos
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (adjacencyMatrix[i][j] == -1) {
                Draw(window, j, i, sf::Color::Red); // Obstáculo
            } else {
                Draw(window, j, i, sf::Color(240, 240, 240)); // Celda normal
            }

            // Dibujar borde de la celda
            sf::RectangleShape border(sf::Vector2f(dw, dh));
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineThickness(1);
            border.setOutlineColor(sf::Color(200, 200, 200)); // Color del borde
            border.setPosition(j * dw, i * dh);
            window.draw(border);
        }
    }


            player1.updatePosition();
            player2.updatePosition();

            // Check if the current player's move is complete
            if (moveInitiated) {
                if (player1Turn && !player1.isMoving()) {
                    player1Turn = false;
                    moveInitiated = false;
                } else if (!player1Turn && !player2.isMoving()) {
                    player1Turn = true;
                    moveInitiated = false;
                }
            }


            // Dibujar los jugadores
            player1.draw(window);
            player2.draw(window);

            window.display();
        }


        matrixThread.join();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
    return 0;
}

