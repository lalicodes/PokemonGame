//AUTHOR: LALITAAMBIGAI BALAKRISHNAN

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

typedef struct Point {
    int y;
    int x;
} Point;

typedef struct Queue {
    Point* points;
    int capacity;
    int size;
    int front;
    int rear;
} Queue;

Queue* CreateQueue(int capacity);
bool IsEmpty(Queue* queue);
bool IsFull(Queue* queue);
void Enqueue(Queue* queue, Point item);
Point Dequeue(Queue* queue);
void FreeQueue(Queue* queue);

#define WIDTH 80
#define HEIGHT 21

#define GRASS ':'
#define WATER '~'
#define PATH '#'
#define BOULDER '%'
#define CLEARING '.'
#define TREE '^'
#define POKEMON_CENTER 'C'
#define POKE_MART 'M'

#define COLOR_RESET   "\x1B[0m"
#define COLOR_GREEN   "\x1B[32m"
#define COLOR_BLUE    "\x1B[34m"
#define COLOR_RED     "\x1B[31m"
#define COLOR_YELLOW  "\x1B[33m"
#define COLOR_CYAN    "\x1B[36m"
#define COLOR_MAGENTA "\x1B[35m"

void printColoredChar(char character) {
    switch (character) {
        case '.': printf(COLOR_GREEN "%c" COLOR_RESET, character); break;
        case '#': printf(COLOR_BLUE "%c" COLOR_RESET, character); break;
        case ':': printf(COLOR_RED "%c" COLOR_RESET, character); break;
        case '~': printf(COLOR_CYAN "%c" COLOR_RESET, character); break;
        case '^': printf(COLOR_YELLOW "%c" COLOR_RESET, character); break;
        case 'C': printf(COLOR_MAGENTA "%c" COLOR_RESET, character); break;
        case 'M': printf(COLOR_MAGENTA "%c" COLOR_RESET, character); break;
        default: printf("%c", character);
    }
}

void printLegend() {
    printf("Legend:\n");
    printf(COLOR_GREEN " . " COLOR_RESET ": Clearing\n");
    printf(COLOR_BLUE " # " COLOR_RESET ": Path\n");
    printf(COLOR_RED " : " COLOR_RESET ": Tall Grass\n");
    printf(COLOR_CYAN " ~ " COLOR_RESET ": Water\n");
    printf(COLOR_YELLOW " ^ " COLOR_RESET ": Tree \n");
    printf(COLOR_MAGENTA " C " COLOR_RESET ": Pokémon Center\n");
    printf(COLOR_MAGENTA " M " COLOR_RESET ": Pokémon Mart\n");
}

Queue* CreateQueue(int capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->points = (struct Point*)malloc(sizeof(struct Point) * capacity);
    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    queue->rear = -1;
    return queue;
}

bool IsEmpty(Queue* queue) {
    return queue->size == 0;
}

bool IsFull(Queue* queue) {
    return queue->size == queue->capacity;
}

void Enqueue(Queue* queue, struct Point item) {
    if (IsFull(queue)) return;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->points[queue->rear] = item;
    queue->size++;
}

struct Point Dequeue(Queue* queue) {
    struct Point item = {0, 0};
    if (IsEmpty(queue)) return item;
    item = queue->points[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return item;
}

void FreeQueue(Queue* queue) {
    free(queue->points);
    free(queue);
}

void InitializeMap(char map[HEIGHT][WIDTH]) {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            if (i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
                map[i][j] = BOULDER;
            } else {
                map[i][j] = CLEARING;
            }
        }
    }

    int northExitX = rand() % (WIDTH - 2) + 1; 
    int southExitX = rand() % (WIDTH - 2) + 1; 
    int westExitY = rand() % (HEIGHT - 2) + 1; 
    int eastExitY = rand() % (HEIGHT - 2) + 1; 

    map[0][northExitX] = BOULDER;
    map[HEIGHT - 1][southExitX] = BOULDER;
    map[westExitY][0] = BOULDER;
    map[eastExitY][WIDTH - 1] = BOULDER;
}

void PrintMap(char map[HEIGHT][WIDTH]) {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            printColoredChar(map[i][j]);
        }
        printf(COLOR_RESET "\n");
    }
}

void AddPaths(char map[HEIGHT][WIDTH]) {

    for (int i = 0; i < HEIGHT; i++) {
        if (map[i][WIDTH / 2] != POKEMON_CENTER && map[i][WIDTH / 2] != POKE_MART) {
            map[i][WIDTH / 2] = PATH;
        } else {
            if (WIDTH / 2 + 1 < WIDTH && map[i][WIDTH / 2 + 1] == CLEARING) {
                map[i][WIDTH / 2 + 1] = PATH; 
            } else if (WIDTH / 2 - 1 >= 0 && map[i][WIDTH / 2 - 1] == CLEARING) {
                map[i][WIDTH / 2 - 1] = PATH; 
            }
        }
    }

    for (int j = 0; j < WIDTH; j++) {
        if (map[HEIGHT / 2][j] != POKEMON_CENTER && map[HEIGHT / 2][j] != POKE_MART) {
            map[HEIGHT / 2][j] = PATH;
        } else {
            
            if (HEIGHT / 2 + 1 < HEIGHT && map[HEIGHT / 2 + 1][j] == CLEARING) {
                map[HEIGHT / 2 + 1][j] = PATH;  
            } else if (HEIGHT / 2 - 1 >= 0 && map[HEIGHT / 2 - 1][j] == CLEARING) {
                map[HEIGHT / 2 - 1][j] = PATH; 
            }
        }
    }
}


void ExpandArea(char map[HEIGHT][WIDTH], int startY, int startX, char terrain, int size) {
    Queue* queue = CreateQueue(size * 4);
    Enqueue(queue, (struct Point){startY, startX});

    int directions[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}}; 
    int count = 0;

    while (!IsEmpty(queue) && count < size) {
        struct Point current = Dequeue(queue);
        int y = current.y;
        int x = current.x;

        if (map[y][x] == CLEARING) {
            map[y][x] = terrain;
            count++;

            for (int i = 0; i < 4; i++) { 
                int newY = y + directions[i][0];
                int newX = x + directions[i][1];

                if (newY >= 0 && newY < HEIGHT && newX >= 0 && newX < WIDTH && map[newY][newX] == CLEARING) {
                    Enqueue(queue, (struct Point){newY, newX});
                }
            }
        }
    }

    FreeQueue(queue);
}

void AddFeatures(char map[HEIGHT][WIDTH]) {
    printf("Adding features...\n");

    // Randomly place two tall grass areas.
    for (int i = 0; i < 2; i++) {
        int grassSize = rand() % 50 + 30; 
        int startY = rand() % (HEIGHT - 2) + 2;
        int startX = rand() % (WIDTH - 2) + 2; 
        ExpandArea(map, startY, startX, GRASS, grassSize);
    }

    // Randomly place a water area.
    int waterSize = rand() % 300 + 100; 
    int startY = rand() % (HEIGHT - 2) + 1; 
    int startX = rand() % (WIDTH - 2) + 1;
    ExpandArea(map, startY, startX, WATER, waterSize);

    // Add trees.
    int treeCount = rand() % 50 + 20; 
    for (int i = 0; i < treeCount; i++) {
        int y = rand() % (HEIGHT - 2) + 1; 
        int x = rand() % (WIDTH - 2) + 1; 
        if (map[y][x] == CLEARING) {
            map[y][x] = TREE;
        }
    }

    printf("Features added!\n");
}

void AddPokemonCenter(char map[HEIGHT][WIDTH]) {
    int y, x;
    do {
        y = rand() % (HEIGHT - 2) + 1; 
        x = rand() % (WIDTH - 2) + 1; 
    } while (map[y][x] != CLEARING || map[y + 1][x] != CLEARING || map[y][x + 1] != CLEARING || map[y + 1][x + 1] != CLEARING);

    map[y][x] = POKEMON_CENTER;
    map[y + 1][x] = POKEMON_CENTER;
    map[y][x + 1] = POKEMON_CENTER;
    map[y + 1][x + 1] = POKEMON_CENTER;
    y += 2;

    while (map[y][x] != PATH) {
        map[y][x] = PATH;
        x++;
    }
}

void AddPokemonMart(char map[HEIGHT][WIDTH]) {
    int y, x;
    do {
        y = rand() % (HEIGHT - 2) + 1; 
        x = rand() % (WIDTH - 2) + 1; 
    } while (map[y][x] != CLEARING || map[y + 1][x] != CLEARING || map[y][x + 1] != CLEARING || map[y + 1][x + 1] != CLEARING);

    map[y][x] = POKE_MART;
    map[y + 1][x] = POKE_MART;
    map[y][x + 1] = POKE_MART;
    map[y + 1][x + 1] = POKE_MART;
    y += 2;

    while (map[y][x] != PATH) {
        map[y][x] = PATH;
        x++;
    }
}

int main() {
    srand(time(NULL));

    char map[HEIGHT][WIDTH];
    InitializeMap(map);
    AddPaths(map);
    AddFeatures(map);
    AddPokemonCenter(map);
    AddPokemonMart(map);

    printLegend();
    PrintMap(map);

    return 0;
}
