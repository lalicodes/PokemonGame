#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

int write_pgm(char *file, void *image, uint32_t x, uint32_t y) {
    FILE *o;

    if (!(o = fopen(file, "wb"))) {
        perror(file);
        return -1;
    }

    fprintf(o, "P5\n%u %u\n255\n", x, y);

    if (fwrite(image, 1, x * y, o) != (x * y)) {
        perror("fwrite");
        fclose(o);
        return -1;
    }

    fclose(o);
    return 0;
}

int read_pgm(char *file, void *image, uint32_t x, uint32_t y) {
    FILE *f;
    char s[80];
    unsigned i, j;

    if (!(f = fopen(file, "rb"))) {
        perror(file);
        printf("Error: %s\n", file);
        return -1;
    }

    if (!fgets(s, 80, f) || strncmp(s, "P5", 2)) {
        fprintf(stderr, "Expected P5\n");
        fclose(f);
        return -1;
    }

    do {
        fgets(s, 80, f);
    } while (s[0] == '#');

    if (sscanf(s, "%u %u", &i, &j) != 2 || i != x || j != y) {
        fprintf(stderr, "Expected x and y dimensions %u %u\n", x, y);
        fclose(f);
        return -1;
    }

    do {
        fgets(s, 80, f);
    } while (s[0] == '#');

    if (strncmp(s, "255", 3)) {
        fprintf(stderr, "Expected 255\n");
        fclose(f);
        return -1;
    }

    if (fread(image, 1, x * y, f) != x * y) {
        perror("fread");
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

void apply_sobel(uint8_t image[1024][1024], uint8_t out[1024][1024]) {
    int8_t Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int8_t Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    for (int x = 1; x < 1023; x++) {
        for (int y = 1; y < 1023; y++) {
            int sumX = 0;
            int sumY = 0;

            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    int posX = x + i;
                    int posY = y + j;
                    sumX += image[posX][posY] * Gx[i + 1][j + 1];
                    sumY += image[posX][posY] * Gy[i + 1][j + 1];
                }
            }

            int magnitude = (int)sqrt(sumX * sumX + sumY * sumY);
            magnitude = (magnitude > 255) ? 255 : magnitude;  // Clamp to max 255
            out[x][y] = (uint8_t)magnitude;
        }
    }

    for (int i = 0; i < 1024; i++) {
        out[i][0] = out[i][1023] = out[0][i] = out[1023][i] = 0;
    }
}
int main() {
    uint8_t image[1024][1024];
    uint8_t out[1024][1024];

    // Process "motorcycle.pgm"
    if (read_pgm("motorcycle.pgm", image, 1024, 1024) != 0) {
        fprintf(stderr, "Failed to read the motorcycle image\n");
        return -1;
    }

    apply_sobel(image, out);

    if (write_pgm("motorcycle.edge.pgm", out, 1024, 1024) != 0) {
        fprintf(stderr, "Failed to write the motorcycle edge image\n");
        return -1;
    }

    // Process "bigger_digger.pgm"
    if (read_pgm("bigger_digger.pgm", image, 1024, 1024) != 0) {
        fprintf(stderr, "Failed to read the bigger_digger image\n");
        return -1;
    }

    apply_sobel(image, out);

    if (write_pgm("bigger_digger.edge.pgm", out, 1024, 1024) != 0) {
        fprintf(stderr, "Failed to write the bigger_digger edge image\n");
        return -1;
    }

    printf("Edge detection completed successfully.\n");
    return 0;
}
