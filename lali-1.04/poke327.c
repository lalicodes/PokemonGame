#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>

#include "heap.h"

#define malloc(size) ({          \
  void *_tmp;                    \
  assert((_tmp = malloc(size))); \
  _tmp;                          \
})

typedef struct path {
  heap_node_t *hn;
  uint8_t pos[2];
  uint8_t from[2];
  int32_t cost;
} path_t;

typedef enum dim {
  dim_x,
  dim_y,
  num_dims
} dim_t;

typedef int16_t pair_t[num_dims];

#define MAP_X              80
#define MAP_Y              21
#define MIN_TREES          10
#define MIN_BOULDERS       10
#define TREE_PROB          95
#define BOULDER_PROB       95
#define WORLD_SIZE         401

#define mappair(pair) (m->map[pair[dim_y]][pair[dim_x]])
#define mapxy(x, y) (m->map[y][x])
#define heightpair(pair) (m->height[pair[dim_y]][pair[dim_x]])
#define heightxy(x, y) (m->height[y][x])

typedef enum __attribute__ ((__packed__)) terrain_type {
  ter_boulder,
  ter_tree,
  ter_path,
  ter_mart,
  ter_center,
  ter_grass,
  ter_clearing,
  ter_mountain,
  ter_forest,
  num_terrain_types
} terrain_type_t;

typedef enum __attribute__ ((__packed__)) character_type {
  char_pc,
  char_hiker,
  char_rival,
  char_other,
  num_character_types
} character_type_t;

typedef struct trainers {

    pair_t pos;
    int cost;
    int location;
    int direction;
    char trainers;

} trainers_t;

typedef struct pc {
  pair_t pos;
} pc_t;

typedef struct map {
  terrain_type_t map[MAP_Y][MAP_X];
  uint8_t height[MAP_Y][MAP_X];
  int8_t n, s, e, w;
} map_t;

typedef struct queue_node {
  int x, y;
  struct queue_node *next;
} queue_node_t;

typedef struct world {
  map_t *world[WORLD_SIZE][WORLD_SIZE];
  pair_t cur_idx;
  map_t *cur_map;
  /* Please distance maps in world, not map, since *
   * we only need one pair at any given time.      */
  int hiker_dist[MAP_Y][MAP_X];
  int rival_dist[MAP_Y][MAP_X];
  pc_t pc;
} world_t;



/* Even unallocated, a WORLD_SIZE x WORLD_SIZE array of pointers is a very *
 * large thing to put on the stack.  To avoid that, world is a global.     */
world_t world;

int numtrainers;
char map[MAP_Y][MAP_X];

static int32_t move_cost[num_character_types][num_terrain_types] = {
  { INT_MAX, INT_MAX, 10, 10,      10,      20, 10, INT_MAX, INT_MAX },
  { INT_MAX, INT_MAX, 10, INT_MAX, INT_MAX, 15, 10, 15,      15      },
  { INT_MAX, INT_MAX, 10, INT_MAX, INT_MAX, 20, 10, INT_MAX, INT_MAX },
  { INT_MAX, INT_MAX, 10, INT_MAX, INT_MAX, 20, 10, INT_MAX, INT_MAX },
};

static int32_t trainers_cmp(const void *key, const void *with)
{
    return ((trainers_t *)key)->cost - ((trainers_t *)with)->cost;
}

static int32_t path_cmp(const void *key, const void *with) {
  return ((path_t *) key)->cost - ((path_t *) with)->cost;
}

static int32_t edge_penalty(int8_t x, int8_t y)
{
  return (x == 1 || y == 1 || x == MAP_X - 2 || y == MAP_Y - 2) ? 2 : 1;
}

static void dijkstra_path(map_t *m, pair_t from, pair_t to)
{
  static path_t path[MAP_Y][MAP_X], *p;
  static uint32_t initialized = 0;
  heap_t h;
  uint32_t x, y;

  if (!initialized) {
    for (y = 0; y < MAP_Y; y++) {
      for (x = 0; x < MAP_X; x++) {
        path[y][x].pos[dim_y] = y;
        path[y][x].pos[dim_x] = x;
      }
    }
    initialized = 1;
  }

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      path[y][x].cost = INT_MAX;
    }
  }

  path[from[dim_y]][from[dim_x]].cost = 0;

  heap_init(&h, path_cmp, NULL);

  for (y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      path[y][x].hn = heap_insert(&h, &path[y][x]);
    }
  }

  while ((p = heap_remove_min(&h))) {
    p->hn = NULL;

    if ((p->pos[dim_y] == to[dim_y]) && p->pos[dim_x] == to[dim_x]) {
      for (x = to[dim_x], y = to[dim_y];
           (x != from[dim_x]) || (y != from[dim_y]);
           p = &path[y][x], x = p->from[dim_x], y = p->from[dim_y]) {
        mapxy(x, y) = ter_path;
        heightxy(x, y) = 0;
      }
      heap_delete(&h);
      return;
    }

    if ((path[p->pos[dim_y] - 1][p->pos[dim_x]    ].hn) &&
        (path[p->pos[dim_y] - 1][p->pos[dim_x]    ].cost >
         ((p->cost + heightpair(p->pos)) *
          edge_penalty(p->pos[dim_x], p->pos[dim_y] - 1)))) {
      path[p->pos[dim_y] - 1][p->pos[dim_x]    ].cost =
        ((p->cost + heightpair(p->pos)) *
         edge_penalty(p->pos[dim_x], p->pos[dim_y] - 1));
      path[p->pos[dim_y] - 1][p->pos[dim_x]    ].from[dim_y] = p->pos[dim_y];
      path[p->pos[dim_y] - 1][p->pos[dim_x]    ].from[dim_x] = p->pos[dim_x];
      heap_decrease_key_no_replace(&h, path[p->pos[dim_y] - 1]
                                           [p->pos[dim_x]    ].hn);
    }
    if ((path[p->pos[dim_y]    ][p->pos[dim_x] - 1].hn) &&
        (path[p->pos[dim_y]    ][p->pos[dim_x] - 1].cost >
         ((p->cost + heightpair(p->pos)) *
          edge_penalty(p->pos[dim_x] - 1, p->pos[dim_y])))) {
      path[p->pos[dim_y]][p->pos[dim_x] - 1].cost =
        ((p->cost + heightpair(p->pos)) *
         edge_penalty(p->pos[dim_x] - 1, p->pos[dim_y]));
      path[p->pos[dim_y]    ][p->pos[dim_x] - 1].from[dim_y] = p->pos[dim_y];
      path[p->pos[dim_y]    ][p->pos[dim_x] - 1].from[dim_x] = p->pos[dim_x];
      heap_decrease_key_no_replace(&h, path[p->pos[dim_y]    ]
                                           [p->pos[dim_x] - 1].hn);
    }
    if ((path[p->pos[dim_y]    ][p->pos[dim_x] + 1].hn) &&
        (path[p->pos[dim_y]    ][p->pos[dim_x] + 1].cost >
         ((p->cost + heightpair(p->pos)) *
          edge_penalty(p->pos[dim_x] + 1, p->pos[dim_y])))) {
      path[p->pos[dim_y]][p->pos[dim_x] + 1].cost =
        ((p->cost + heightpair(p->pos)) *
         edge_penalty(p->pos[dim_x] + 1, p->pos[dim_y]));
      path[p->pos[dim_y]    ][p->pos[dim_x] + 1].from[dim_y] = p->pos[dim_y];
      path[p->pos[dim_y]    ][p->pos[dim_x] + 1].from[dim_x] = p->pos[dim_x];
      heap_decrease_key_no_replace(&h, path[p->pos[dim_y]    ]
                                           [p->pos[dim_x] + 1].hn);
    }
    if ((path[p->pos[dim_y] + 1][p->pos[dim_x]    ].hn) &&
        (path[p->pos[dim_y] + 1][p->pos[dim_x]    ].cost >
         ((p->cost + heightpair(p->pos)) *
          edge_penalty(p->pos[dim_x], p->pos[dim_y] + 1)))) {
      path[p->pos[dim_y] + 1][p->pos[dim_x]    ].cost =
        ((p->cost + heightpair(p->pos)) *
         edge_penalty(p->pos[dim_x], p->pos[dim_y] + 1));
      path[p->pos[dim_y] + 1][p->pos[dim_x]    ].from[dim_y] = p->pos[dim_y];
      path[p->pos[dim_y] + 1][p->pos[dim_x]    ].from[dim_x] = p->pos[dim_x];
      heap_decrease_key_no_replace(&h, path[p->pos[dim_y] + 1]
                                           [p->pos[dim_x]    ].hn);
    }
  }
}

static int build_paths(map_t *m)
{
  pair_t from, to;

  /*  printf("%d %d %d %d\n", m->n, m->s, m->e, m->w);*/

  if (m->e != -1 && m->w != -1) {
    from[dim_x] = 1;
    to[dim_x] = MAP_X - 2;
    from[dim_y] = m->w;
    to[dim_y] = m->e;

    dijkstra_path(m, from, to);
  }

  if (m->n != -1 && m->s != -1) {
    from[dim_y] = 1;
    to[dim_y] = MAP_Y - 2;
    from[dim_x] = m->n;
    to[dim_x] = m->s;

    dijkstra_path(m, from, to);
  }

  if (m->e == -1) {
    if (m->s == -1) {
      from[dim_x] = 1;
      from[dim_y] = m->w;
      to[dim_x] = m->n;
      to[dim_y] = 1;
    } else {
      from[dim_x] = 1;
      from[dim_y] = m->w;
      to[dim_x] = m->s;
      to[dim_y] = MAP_Y - 2;
    }

    dijkstra_path(m, from, to);
  }

  if (m->w == -1) {
    if (m->s == -1) {
      from[dim_x] = MAP_X - 2;
      from[dim_y] = m->e;
      to[dim_x] = m->n;
      to[dim_y] = 1;
    } else {
      from[dim_x] = MAP_X - 2;
      from[dim_y] = m->e;
      to[dim_x] = m->s;
      to[dim_y] = MAP_Y - 2;
    }

    dijkstra_path(m, from, to);
  }

  if (m->n == -1) {
    if (m->e == -1) {
      from[dim_x] = 1;
      from[dim_y] = m->w;
      to[dim_x] = m->s;
      to[dim_y] = MAP_Y - 2;
    } else {
      from[dim_x] = MAP_X - 2;
      from[dim_y] = m->e;
      to[dim_x] = m->s;
      to[dim_y] = MAP_Y - 2;
    }

    dijkstra_path(m, from, to);
  }

  if (m->s == -1) {
    if (m->e == -1) {
      from[dim_x] = 1;
      from[dim_y] = m->w;
      to[dim_x] = m->n;
      to[dim_y] = 1;
    } else {
      from[dim_x] = MAP_X - 2;
      from[dim_y] = m->e;
      to[dim_x] = m->n;
      to[dim_y] = 1;
    }

    dijkstra_path(m, from, to);
  }

  return 0;
}

static int gaussian[5][5] = {
  {  1,  4,  7,  4,  1 },
  {  4, 16, 26, 16,  4 },
  {  7, 26, 41, 26,  7 },
  {  4, 16, 26, 16,  4 },
  {  1,  4,  7,  4,  1 }
};

static int smooth_height(map_t *m)
{
  int32_t i, x, y;
  int32_t s, t, p, q;
  queue_node_t *head, *tail, *tmp;
  /*  FILE *out;*/
  uint8_t height[MAP_Y][MAP_X];

  memset(&height, 0, sizeof (height));

  /* Seed with some values */
  for (i = 1; i < 255; i += 20) {
    do {
      x = rand() % MAP_X;
      y = rand() % MAP_Y;
    } while (height[y][x]);
    height[y][x] = i;
    if (i == 1) {
      head = tail = malloc(sizeof (*tail));
    } else {
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
    }
    tail->next = NULL;
    tail->x = x;
    tail->y = y;
  }

  /*
  out = fopen("seeded.pgm", "w");
  fprintf(out, "P5\n%u %u\n255\n", MAP_X, MAP_Y);
  fwrite(&height, sizeof (height), 1, out);
  fclose(out);
  */

  /* Diffuse the vaules to fill the space */
  while (head) {
    x = head->x;
    y = head->y;
    i = height[y][x];

    if (x - 1 >= 0 && y - 1 >= 0 && !height[y - 1][x - 1]) {
      height[y - 1][x - 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x - 1;
      tail->y = y - 1;
    }
    if (x - 1 >= 0 && !height[y][x - 1]) {
      height[y][x - 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x - 1;
      tail->y = y;
    }
    if (x - 1 >= 0 && y + 1 < MAP_Y && !height[y + 1][x - 1]) {
      height[y + 1][x - 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x - 1;
      tail->y = y + 1;
    }
    if (y - 1 >= 0 && !height[y - 1][x]) {
      height[y - 1][x] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x;
      tail->y = y - 1;
    }
    if (y + 1 < MAP_Y && !height[y + 1][x]) {
      height[y + 1][x] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x;
      tail->y = y + 1;
    }
    if (x + 1 < MAP_X && y - 1 >= 0 && !height[y - 1][x + 1]) {
      height[y - 1][x + 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x + 1;
      tail->y = y - 1;
    }
    if (x + 1 < MAP_X && !height[y][x + 1]) {
      height[y][x + 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x + 1;
      tail->y = y;
    }
    if (x + 1 < MAP_X && y + 1 < MAP_Y && !height[y + 1][x + 1]) {
      height[y + 1][x + 1] = i;
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
      tail->next = NULL;
      tail->x = x + 1;
      tail->y = y + 1;
    }

    tmp = head;
    head = head->next;
    free(tmp);
  }

  /* And smooth it a bit with a gaussian convolution */
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      for (s = t = p = 0; p < 5; p++) {
        for (q = 0; q < 5; q++) {
          if (y + (p - 2) >= 0 && y + (p - 2) < MAP_Y &&
              x + (q - 2) >= 0 && x + (q - 2) < MAP_X) {
            s += gaussian[p][q];
            t += height[y + (p - 2)][x + (q - 2)] * gaussian[p][q];
          }
        }
      }
      m->height[y][x] = t / s;
    }
  }
  /* Let's do it again, until it's smooth like Kenny G. */
  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      for (s = t = p = 0; p < 5; p++) {
        for (q = 0; q < 5; q++) {
          if (y + (p - 2) >= 0 && y + (p - 2) < MAP_Y &&
              x + (q - 2) >= 0 && x + (q - 2) < MAP_X) {
            s += gaussian[p][q];
            t += height[y + (p - 2)][x + (q - 2)] * gaussian[p][q];
          }
        }
      }
      m->height[y][x] = t / s;
    }
  }

  /*
  out = fopen("diffused.pgm", "w");
  fprintf(out, "P5\n%u %u\n255\n", MAP_X, MAP_Y);
  fwrite(&height, sizeof (height), 1, out);
  fclose(out);

  out = fopen("smoothed.pgm", "w");
  fprintf(out, "P5\n%u %u\n255\n", MAP_X, MAP_Y);
  fwrite(&m->height, sizeof (m->height), 1, out);
  fclose(out);
  */

  return 0;
}

static void find_building_location(map_t *m, pair_t p)
{
  do {
    p[dim_x] = rand() % (MAP_X - 5) + 3;
    p[dim_y] = rand() % (MAP_Y - 10) + 5;

    if ((((mapxy(p[dim_x] - 1, p[dim_y]    ) == ter_path)     &&
          (mapxy(p[dim_x] - 1, p[dim_y] + 1) == ter_path))    ||
         ((mapxy(p[dim_x] + 2, p[dim_y]    ) == ter_path)     &&
          (mapxy(p[dim_x] + 2, p[dim_y] + 1) == ter_path))    ||
         ((mapxy(p[dim_x]    , p[dim_y] - 1) == ter_path)     &&
          (mapxy(p[dim_x] + 1, p[dim_y] - 1) == ter_path))    ||
         ((mapxy(p[dim_x]    , p[dim_y] + 2) == ter_path)     &&
          (mapxy(p[dim_x] + 1, p[dim_y] + 2) == ter_path)))   &&
        (((mapxy(p[dim_x]    , p[dim_y]    ) != ter_mart)     &&
          (mapxy(p[dim_x]    , p[dim_y]    ) != ter_center)   &&
          (mapxy(p[dim_x] + 1, p[dim_y]    ) != ter_mart)     &&
          (mapxy(p[dim_x] + 1, p[dim_y]    ) != ter_center)   &&
          (mapxy(p[dim_x]    , p[dim_y] + 1) != ter_mart)     &&
          (mapxy(p[dim_x]    , p[dim_y] + 1) != ter_center)   &&
          (mapxy(p[dim_x] + 1, p[dim_y] + 1) != ter_mart)     &&
          (mapxy(p[dim_x] + 1, p[dim_y] + 1) != ter_center))) &&
        (((mapxy(p[dim_x]    , p[dim_y]    ) != ter_path)     &&
          (mapxy(p[dim_x] + 1, p[dim_y]    ) != ter_path)     &&
          (mapxy(p[dim_x]    , p[dim_y] + 1) != ter_path)     &&
          (mapxy(p[dim_x] + 1, p[dim_y] + 1) != ter_path)))) {
          break;
    }
  } while (1);
}

static int place_pokemart(map_t *m)
{
  pair_t p;

  find_building_location(m, p);

  mapxy(p[dim_x]    , p[dim_y]    ) = ter_mart;
  mapxy(p[dim_x] + 1, p[dim_y]    ) = ter_mart;
  mapxy(p[dim_x]    , p[dim_y] + 1) = ter_mart;
  mapxy(p[dim_x] + 1, p[dim_y] + 1) = ter_mart;

  return 0;
}

static int place_center(map_t *m)
{  pair_t p;

  find_building_location(m, p);

  mapxy(p[dim_x]    , p[dim_y]    ) = ter_center;
  mapxy(p[dim_x] + 1, p[dim_y]    ) = ter_center;
  mapxy(p[dim_x]    , p[dim_y] + 1) = ter_center;
  mapxy(p[dim_x] + 1, p[dim_y] + 1) = ter_center;

  return 0;
}

static int map_terrain(map_t *m, int8_t n, int8_t s, int8_t e, int8_t w)
{
  int32_t i, x, y;
  queue_node_t *head, *tail, *tmp;
  //  FILE *out;
  int num_grass, num_clearing, num_mountain, num_forest, num_total;
  terrain_type_t type;
  int added_current = 0;

  num_grass = rand() % 4 + 2;
  num_clearing = rand() % 4 + 2;
  num_mountain = rand() % 2 + 1;
  num_forest = rand() % 2 + 1;
  num_total = num_grass + num_clearing + num_mountain + num_forest;

  memset(&m->map, 0, sizeof (m->map));

  /* Seed with some values */
  for (i = 0; i < num_total; i++) {
    do {
      x = rand() % MAP_X;
      y = rand() % MAP_Y;
    } while (m->map[y][x]);
    if (i == 0) {
      type = ter_grass;
    } else if (i == num_grass) {
      type = ter_clearing;
    } else if (i == num_grass + num_clearing) {
      type = ter_mountain;
    } else if (i == num_grass + num_clearing + num_mountain) {
      type = ter_forest;
    }
    m->map[y][x] = type;
    if (i == 0) {
      head = tail = malloc(sizeof (*tail));
    } else {
      tail->next = malloc(sizeof (*tail));
      tail = tail->next;
    }
    tail->next = NULL;
    tail->x = x;
    tail->y = y;
  }

  /*
  out = fopen("seeded.pgm", "w");
  fprintf(out, "P5\n%u %u\n255\n", MAP_X, MAP_Y);
  fwrite(&m->map, sizeof (m->map), 1, out);
  fclose(out);
  */

  /* Diffuse the vaules to fill the space */
  while (head) {
    x = head->x;
    y = head->y;
    i = m->map[y][x];

    if (x - 1 >= 0 && !m->map[y][x - 1]) {
      if ((rand() % 100) < 80) {
        m->map[y][x - 1] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x - 1;
        tail->y = y;
      } else if (!added_current) {
        added_current = 1;
        m->map[y][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y;
      }
    }

    if (y - 1 >= 0 && !m->map[y - 1][x]) {
      if ((rand() % 100) < 20) {
        m->map[y - 1][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y - 1;
      } else if (!added_current) {
        added_current = 1;
        m->map[y][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y;
      }
    }

    if (y + 1 < MAP_Y && !m->map[y + 1][x]) {
      if ((rand() % 100) < 20) {
        m->map[y + 1][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y + 1;
      } else if (!added_current) {
        added_current = 1;
        m->map[y][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y;
      }
    }

    if (x + 1 < MAP_X && !m->map[y][x + 1]) {
      if ((rand() % 100) < 80) {
        m->map[y][x + 1] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x + 1;
        tail->y = y;
      } else if (!added_current) {
        added_current = 1;
        m->map[y][x] = i;
        tail->next = malloc(sizeof (*tail));
        tail = tail->next;
        tail->next = NULL;
        tail->x = x;
        tail->y = y;
      }
    }

    added_current = 0;
    tmp = head;
    head = head->next;
    free(tmp);
  }

  /*
  out = fopen("diffused.pgm", "w");
  fprintf(out, "P5\n%u %u\n255\n", MAP_X, MAP_Y);
  fwrite(&m->map, sizeof (m->map), 1, out);
  fclose(out);
  */

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (y == 0 || y == MAP_Y - 1 ||
          x == 0 || x == MAP_X - 1) {
        mapxy(x, y) = ter_boulder;
      }
    }
  }

  m->n = n;
  m->s = s;
  m->e = e;
  m->w = w;

  if (n != -1) {
    mapxy(n,         0        ) = ter_path;
    mapxy(n,         1        ) = ter_path;
  }
  if (s != -1) {
    mapxy(s,         MAP_Y - 1) = ter_path;
    mapxy(s,         MAP_Y - 2) = ter_path;
  }
  if (w != -1) {
    mapxy(0,         w        ) = ter_path;
    mapxy(1,         w        ) = ter_path;
  }
  if (e != -1) {
    mapxy(MAP_X - 1, e        ) = ter_path;
    mapxy(MAP_X - 2, e        ) = ter_path;
  }

  return 0;
}

static int place_boulders(map_t *m)
{
  int i;
  int x, y;

  for (i = 0; i < MIN_BOULDERS || rand() % 100 < BOULDER_PROB; i++) {
    y = rand() % (MAP_Y - 2) + 1;
    x = rand() % (MAP_X - 2) + 1;
    if (m->map[y][x] != ter_forest && m->map[y][x] != ter_path) {
      m->map[y][x] = ter_boulder;
    }
  }

  return 0;
}

static int place_trees(map_t *m)
{
  int i;
  int x, y;

  for (i = 0; i < MIN_TREES || rand() % 100 < TREE_PROB; i++) {
    y = rand() % (MAP_Y - 2) + 1;
    x = rand() % (MAP_X - 2) + 1;
    if (m->map[y][x] != ter_mountain && m->map[y][x] != ter_path) {
      m->map[y][x] = ter_tree;
    }
  }

  return 0;
}

// New map expects cur_idx to refer to the index to be generated.  If that
// map has already been generated then the only thing this does is set
// cur_map.
static int new_map()
{
  int d, p;
  int e, w, n, s;

  if (world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x]]) {
    world.cur_map = world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x]];
    return 0;
  }

  world.cur_map                                             =
    world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x]] =
    malloc(sizeof (*world.cur_map));

  smooth_height(world.cur_map);

  if (!world.cur_idx[dim_y]) {
    n = -1;
  } else if (world.world[world.cur_idx[dim_y] - 1][world.cur_idx[dim_x]]) {
    n = world.world[world.cur_idx[dim_y] - 1][world.cur_idx[dim_x]]->s;
  } else {
    n = 1 + rand() % (MAP_X - 2);
  }
  if (world.cur_idx[dim_y] == WORLD_SIZE - 1) {
    s = -1;
  } else if (world.world[world.cur_idx[dim_y] + 1][world.cur_idx[dim_x]]) {
    s = world.world[world.cur_idx[dim_y] + 1][world.cur_idx[dim_x]]->n;
  } else  {
    s = 1 + rand() % (MAP_X - 2);
  }
  if (!world.cur_idx[dim_x]) {
    w = -1;
  } else if (world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x] - 1]) {
    w = world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x] - 1]->e;
  } else {
    w = 1 + rand() % (MAP_Y - 2);
  }
  if (world.cur_idx[dim_x] == WORLD_SIZE - 1) {
    e = -1;
  } else if (world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x] + 1]) {
    e = world.world[world.cur_idx[dim_y]][world.cur_idx[dim_x] + 1]->w;
  } else {
    e = 1 + rand() % (MAP_Y - 2);
  }

  map_terrain(world.cur_map, n, s, e, w);

  place_boulders(world.cur_map);
  place_trees(world.cur_map);
  build_paths(world.cur_map);
  d = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
       abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
  p = d > 200 ? 5 : (50 - ((45 * d) / 200));
  //  printf("d=%d, p=%d\n", d, p);
  if ((rand() % 100) < p || !d) {
    place_pokemart(world.cur_map);
  }
  if ((rand() % 100) < p || !d) {
    place_center(world.cur_map);
  }

  return 0;
}

static void print_map(trainers_t trainers[])
{
  int x, y, i;
  int default_reached = 0;

  printf("\n\n\n");

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.pc.pos[dim_x] == x &&
          world.pc.pos[dim_y] == y) {
          map[y][x] = '@';
      } else {
        switch (world.cur_map->map[y][x]) {
        case ter_boulder:
        case ter_mountain:
            map[y][x] = '%';
          break;
        case ter_tree:
        case ter_forest:
            map[y][x] = '^';
          break;
        case ter_path:
            map[y][x] = '#';
          break;
        case ter_mart:
            map[y][x] = 'M';
          break;
        case ter_center:
            map[y][x] = 'C';
          break;
        case ter_grass:
            map[y][x] = ':';
          break;
        case ter_clearing:
            map[y][x] = '.';
          break;
        default:
          default_reached = 1;
          break;
        }
      }
    }

  }

  for(i = 0; i < numtrainers; i++)
  {
      map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]] = trainers[i].trainers;
  }

  for(y = 0; y < MAP_Y; y++)
  {
      for(x = 0; x < MAP_X; x++)
      {
          printf("%c", map[y][x]);
      }

      printf("\n");
  }

  if (default_reached) {
    fprintf(stderr, "Default reached in %s\n", __FUNCTION__);
  }
}

// The world is global because of its size, so init_world is parameterless
void init_world()
{
  world.cur_idx[dim_x] = world.cur_idx[dim_y] = WORLD_SIZE / 2;
  new_map();
}

void delete_world()
{
  int x, y;

  for (y = 0; y < WORLD_SIZE; y++) {
    for (x = 0; x < WORLD_SIZE; x++) {
      if (world.world[y][x]) {
        free(world.world[y][x]);
        world.world[y][x] = NULL;
      }
    }
  }
}

#define ter_cost(x, y, c) move_cost[c][m->map[y][x]]

static int32_t hiker_cmp(const void *key, const void *with) {
  return (world.hiker_dist[((path_t *) key)->pos[dim_y]]
                          [((path_t *) key)->pos[dim_x]] -
          world.hiker_dist[((path_t *) with)->pos[dim_y]]
                          [((path_t *) with)->pos[dim_x]]);
}

static int32_t rival_cmp(const void *key, const void *with) {
  return (world.rival_dist[((path_t *) key)->pos[dim_y]]
                          [((path_t *) key)->pos[dim_x]] -
          world.rival_dist[((path_t *) with)->pos[dim_y]]
                          [((path_t *) with)->pos[dim_x]]);
}

void pathfind(map_t *m)
{
  heap_t h;
  uint32_t x, y;
  static path_t p[MAP_Y][MAP_X], *c;
  static uint32_t initialized = 0;

  if (!initialized) {
    initialized = 1;
    for (y = 0; y < MAP_Y; y++) {
      for (x = 0; x < MAP_X; x++) {
        p[y][x].pos[dim_y] = y;
        p[y][x].pos[dim_x] = x;
      }
    }
  }

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      world.hiker_dist[y][x] = world.rival_dist[y][x] = INT_MAX;
    }
  }
  world.hiker_dist[world.pc.pos[dim_y]][world.pc.pos[dim_x]] =
    world.rival_dist[world.pc.pos[dim_y]][world.pc.pos[dim_x]] = 0;

  heap_init(&h, hiker_cmp, NULL);

  for (y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (ter_cost(x, y, char_hiker) != INT_MAX) {
        p[y][x].hn = heap_insert(&h, &p[y][x]);
      } else {
        p[y][x].hn = NULL;
      }
    }
  }

  while ((c = heap_remove_min(&h))) {
    c->hn = NULL;
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x] - 1].hn) &&
        (world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x] - 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x] - 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x]    ].hn) &&
        (world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x]    ] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x]    ] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x]    ].hn);
    }
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x] + 1].hn) &&
        (world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x] + 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] - 1][c->pos[dim_x] + 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x] + 1].hn);
    }
    if ((p[c->pos[dim_y]    ][c->pos[dim_x] - 1].hn) &&
        (world.hiker_dist[c->pos[dim_y]    ][c->pos[dim_x] - 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y]    ][c->pos[dim_x] - 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y]    ][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y]    ][c->pos[dim_x] + 1].hn) &&
        (world.hiker_dist[c->pos[dim_y]    ][c->pos[dim_x] + 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y]    ][c->pos[dim_x] + 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y]    ][c->pos[dim_x] + 1].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x] - 1].hn) &&
        (world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x] - 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x] - 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x]    ].hn) &&
        (world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x]    ] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x]    ] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x]    ].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x] + 1].hn) &&
        (world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x] + 1] >
         world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker))) {
      world.hiker_dist[c->pos[dim_y] + 1][c->pos[dim_x] + 1] =
        world.hiker_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_hiker);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x] + 1].hn);
    }
  }
  heap_delete(&h);

  heap_init(&h, rival_cmp, NULL);

  for (y = 1; y < MAP_Y - 1; y++) {
    for (x = 1; x < MAP_X - 1; x++) {
      if (ter_cost(x, y, char_rival) != INT_MAX) {
        p[y][x].hn = heap_insert(&h, &p[y][x]);
      } else {
        p[y][x].hn = NULL;
      }
    }
  }

  while ((c = heap_remove_min(&h))) {
    c->hn = NULL;
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x] - 1].hn) &&
        (world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x] - 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x] - 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x]    ].hn) &&
        (world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x]    ] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x]    ] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x]    ].hn);
    }
    if ((p[c->pos[dim_y] - 1][c->pos[dim_x] + 1].hn) &&
        (world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x] + 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] - 1][c->pos[dim_x] + 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] - 1][c->pos[dim_x] + 1].hn);
    }
    if ((p[c->pos[dim_y]    ][c->pos[dim_x] - 1].hn) &&
        (world.rival_dist[c->pos[dim_y]    ][c->pos[dim_x] - 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y]    ][c->pos[dim_x] - 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y]    ][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y]    ][c->pos[dim_x] + 1].hn) &&
        (world.rival_dist[c->pos[dim_y]    ][c->pos[dim_x] + 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y]    ][c->pos[dim_x] + 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y]    ][c->pos[dim_x] + 1].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x] - 1].hn) &&
        (world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x] - 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x] - 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x] - 1].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x]    ].hn) &&
        (world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x]    ] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x]    ] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x]    ].hn);
    }
    if ((p[c->pos[dim_y] + 1][c->pos[dim_x] + 1].hn) &&
        (world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x] + 1] >
         world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
         ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival))) {
      world.rival_dist[c->pos[dim_y] + 1][c->pos[dim_x] + 1] =
        world.rival_dist[c->pos[dim_y]][c->pos[dim_x]] +
        ter_cost(c->pos[dim_x], c->pos[dim_y], char_rival);
      heap_decrease_key_no_replace(&h,
                                   p[c->pos[dim_y] + 1][c->pos[dim_x] + 1].hn);
    }
  }
  heap_delete(&h);
}

void init_pc()
{
  int x, y;

  do {
    x = rand() % (MAP_X - 2) + 1;
    y = rand() % (MAP_Y - 2) + 1;
  } while (world.cur_map->map[y][x] != ter_path);

  world.pc.pos[dim_x] = x;
  world.pc.pos[dim_y] = y;
}

void print_hiker_dist()
{
  int x, y;

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.hiker_dist[y][x] == INT_MAX) {
        printf("   ");
      } else {
        printf(" %02d", world.hiker_dist[y][x] % 100);
      }
    }
    printf("\n");
  }
}

void print_rival_dist()
{
  int x, y;

  for (y = 0; y < MAP_Y; y++) {
    for (x = 0; x < MAP_X; x++) {
      if (world.rival_dist[y][x] == INT_MAX || world.rival_dist[y][x] < 0) {
        printf("   ");
      } else {
        printf(" %02d", world.rival_dist[y][x] % 100);
      }
    }
    printf("\n");
  }
}

void reset_hiker_cost(trainers_t trainers[], int i)
{

    if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]] == ter_forest ||
       world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]] == ter_mountain ||
       world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]] == ter_grass) {

         trainers[i].cost += 15;
    }
    else {
        trainers[i].cost += 10;
    }
}

void reset_trainer_cost(trainers_t trainers[], int i)
{
    if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]] == ter_grass) {
        trainers[i].cost += 20;
    }
    else {
        trainers[i].cost += 10;
    }
}

void move_hiker(trainers_t trainers[], int i){
    int j;
    int new_move = 0;
    int min = world.hiker_dist[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]];
    int new_location[8][2] = {
            {-1, -1},
            {-1, 0},
            {-1, 1},
            {0, -1},
            {0, 1},
            {1, -1},
            {1, 0},
            {1, 1}
    };

    for(j = 0; j < 8; j++) {
        if(world.hiker_dist[trainers[i].pos[dim_y] + new_location[j][0]][trainers[i].pos[dim_x] + new_location[j][1]] <= min) {
            min = world.hiker_dist[trainers[i].pos[dim_y] + new_location[j][0]][trainers[i].pos[dim_x] + new_location[j][1]];
            new_move = j;
        }
    }

    if((trainers[i].pos[dim_x] + new_location[new_move][1] == world.pc.pos[dim_x] && trainers[i].pos[dim_y] + new_location[new_move][0] == world.pc.pos[dim_y]) ||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'h'||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'r'||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'p'||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'w'||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 's'||
    map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'e') {
    }
    else {
        trainers[i].pos[dim_x] += new_location[new_move][1];
        trainers[i].pos[dim_y] += new_location[new_move][0];
    }
}

void move_rival(trainers_t trainers[], int i){
    int j;
    int new_move = 0;
    int min = world.rival_dist[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]];
    int new_location[8][2] = {
            {-1, -1},
            {-1, 0},
            {-1, 1},
            {0, -1},
            {0, 1},
            {1, -1},
            {1, 0},
            {1, 1}
    };

    for(j = 0; j < 8; j++) {
        if(world.rival_dist[trainers[i].pos[dim_y] + new_location[j][0]][trainers[i].pos[dim_x] + new_location[j][1]] <= min) {
            min = world.rival_dist[trainers[i].pos[dim_y] + new_location[j][0]][trainers[i].pos[dim_x] + new_location[j][1]];
            new_move = j;
        }
    }

    if((trainers[i].pos[dim_x] + new_location[new_move][1] == world.pc.pos[dim_x] && trainers[i].pos[dim_y] + new_location[new_move][0] == world.pc.pos[dim_y]) ||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'h'||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'r'||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'p'||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'w'||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 's'||
       map[trainers[i].pos[dim_y] + new_location[new_move][0]][trainers[i].pos[dim_x] + new_location[new_move][1]] == 'e') {
    }
    else {
        trainers[i].pos[dim_x] += new_location[new_move][1];
        trainers[i].pos[dim_y] += new_location[new_move][0];
    }
}

void move_pacer(trainers_t trainers[], int i){

    if(trainers[i].direction == 1) {
        if(world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_forest &&
         world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_mountain &&
         world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_boulder &&
         world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_tree &&
         world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_mart &&
         world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_center &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'h' &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'r' &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'p' &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'w' &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 's' &&
         map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] - 1 != 0) {
                trainers[i].pos[dim_y] -= 1;
            }
            else {
                trainers[i].direction = 3;
            }
        }
        else {
            trainers[i].direction = 3;
        }
    }
    else if(trainers[i].direction == 2) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_center &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'e') {
            if(trainers[i].pos[dim_x] - 1 != 0) {
                trainers[i].pos[dim_x] -= 1;
            }
            else {
                trainers[i].direction = 4;
            }
        }
        else {
            trainers[i].direction = 4;
        }
    }
    else if(trainers[i].direction == 3) {
        if(world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_center &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'h' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'r' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'p' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'w' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 's' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] + 1 != MAP_Y - 1) {
                trainers[i].pos[dim_y] += 1;
            }
            else {
                trainers[i].direction = 1;
            }
        }
        else {
            trainers[i].direction = 1;
        }
    }
    else if(trainers[i].direction == 4) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_center &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'e') {
            if(trainers[i].pos[dim_x] + 1 != MAP_X - 1) {
                trainers[i].pos[dim_x] += 1;
            }
            else {
                trainers[i].direction = 2;
            }
        }
        else {
            trainers[i].direction = 2;
        }
    }

}

void move_wanderer(trainers_t trainers[], int i)
{
    terrain_type_t terrain = world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x]];

    if(trainers[i].direction == 1) {
        if(world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] == terrain &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'h' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'r' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'p' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'w' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 's' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] - 1 != 0) {
                trainers[i].pos[dim_y] -= 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 2) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] == terrain &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'e') {
            if(trainers[i].pos[dim_x] - 1 != 0) {
                trainers[i].pos[dim_x] -= 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 3) {
        if(world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] == terrain &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'h' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'r' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'p' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'w' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 's' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] + 1 != 0) {
                trainers[i].pos[dim_y] += 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 4) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] == terrain &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'e') {
            if(trainers[i].pos[dim_x] + 1 != 0) {
                trainers[i].pos[dim_x] += 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
}

void move_explorer(trainers_t trainers[], int i){
    if(trainers[i].direction == 1) {
        if(world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_boulder  &&
        world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != ter_center &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'h' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'r' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'p' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'w' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 's' &&
        map[trainers[i].pos[dim_y] - 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] - 1 != 0) {
                trainers[i].pos[dim_y] -= 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 2) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != ter_center &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] - 1] != 'e') {
            if(trainers[i].pos[dim_x] - 1 != 0) {
                trainers[i].pos[dim_x] -= 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 3) {
        if(world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != ter_center &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'h' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'r' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'p' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'w' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 's' &&
        map[trainers[i].pos[dim_y] + 1][trainers[i].pos[dim_x]] != 'e') {
            if(trainers[i].pos[dim_y] + 1 != 0) {
                trainers[i].pos[dim_y] += 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
    else if(trainers[i].direction == 4) {
        if(world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_forest &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_mountain &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_boulder &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_tree &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_mart &&
        world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != ter_center &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'h' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'r' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'p' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'w' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 's' &&
        map[trainers[i].pos[dim_y]][trainers[i].pos[dim_x] + 1] != 'e') {
            if(trainers[i].pos[dim_x] + 1 != 0) {
                trainers[i].pos[dim_x] += 1;
            }
            else {
                trainers[i].direction = (rand() % 4) + 1;
            }
        }
        else {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }
}

void set_trainer_cost(trainers_t trainers[] )
{
    int i;
    terrain_type_t terrain;

    for(i = 0; i < numtrainers; i++) {
        terrain = world.cur_map->map[trainers[i].pos[dim_y]][trainers[i].pos[dim_y]];
        if(trainers[i].trainers == 'h') {
            if(terrain == ter_forest || terrain == ter_mountain || terrain == ter_grass) {
                trainers[i].cost = 15;
            }
            else {
                trainers[i].cost = 10;
            }
        }
        else {
            if(terrain == ter_grass) {
                trainers[i].cost = 20;
            }
            else {
                trainers[i].cost = 10;
            }
        }
    }

}

void place_trainers(trainers_t trainers[]){

    int i, x, y, num_hikers, num_rivals, rand_trainer;
    terrain_type_t terrain;

    num_hikers = num_rivals = 0;

    trainers[0].pos[dim_x] = world.pc.pos[dim_x];
    trainers[0].pos[dim_y] = world.pc.pos[dim_y];
    trainers[0].location = 0;
    trainers[0].trainers = '@';

    i = 1;

    while(i < numtrainers) {
        x = rand() % (MAP_X - 2) + 1;
        y = rand() % (MAP_Y - 2) + 1;

        terrain = world.cur_map->map[y][x];

        if(terrain != ter_boulder && terrain != ter_tree && terrain != ter_center && terrain != ter_mart && x != trainers[0].pos[dim_x] && y != trainers[0].pos[dim_y]) {
            if(num_hikers == 0) {
                trainers[i].pos[dim_x] = x;
                trainers[i].pos[dim_y] = y;
                trainers[i].location = i;
                trainers[i].trainers = 'h';
                num_hikers = 1;
                i++;
            }
            else if(num_rivals == 0 && terrain != ter_mountain && terrain != ter_forest) {
                trainers[i].pos[dim_x] = x;
                trainers[i].pos[dim_y] = y;
                trainers[i].location = i;
                trainers[i].trainers = 'r';
                num_rivals = 1;
                i++;
            }
            else if(terrain != ter_mountain && terrain != ter_forest) {
                rand_trainer = rand() % 6;
                if(rand_trainer == 0) {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 'r';
                    i++;
                }
                else if(rand_trainer == 1) {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 'p';
                    i++;
                }
                else if(rand_trainer == 2) {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 'w';
                    i++;
                }
                else if(rand_trainer == 3) {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 's';
                    i++;
                }
                else if(rand_trainer == 4) {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 'h';
                    i++;
                }
                else {
                    trainers[i].pos[dim_x] = x;
                    trainers[i].pos[dim_y] = y;
                    trainers[i].location = i;
                    trainers[i].trainers = 'e';
                    i++;
                }
            }
            else {
                trainers[i].pos[dim_x] = x;
                trainers[i].pos[dim_y] = y;
                trainers[i].location = i;
                trainers[i].trainers = 'h';
                i++;
            }
        }
    }
}

int main(int argc, char *argv[])
{
  struct timeval tv;
  uint32_t seed;
  int i;
  heap_t h;

    if(argc > 2) {
        if(strcmp(argv[1], "--numtrainers") == 0) {
            numtrainers = atoi(argv[2]);
        }
        else {
            numtrainers = 10;
        }
    }
    else {
        numtrainers = 10;
    }

    numtrainers++;

  gettimeofday(&tv, NULL);
  seed = (tv.tv_usec ^ (tv.tv_sec << 20)) & 0xffffffff;

  printf("Using seed: %u\n", seed);
  srand(seed);

  init_world();
  init_pc();
  pathfind(world.cur_map);

    trainers_t trainers[numtrainers], *t_pointer;
    place_trainers(trainers);
    set_trainer_cost(trainers);

    for(i = 0; i < numtrainers; i++) {
        if(trainers[i].trainers == 'p') {
            trainers[i].direction = (rand() % 4) + 1;
        }
        else if(trainers[i].trainers == 'w') {
            trainers[i].direction = (rand() % 4) + 1;

        }
        else if(trainers[i].trainers == 'e') {
            trainers[i].direction = (rand() % 4) + 1;
        }
    }


    print_map(trainers);

    heap_init(&h, trainers_cmp, NULL);
    for(i = 0; i < numtrainers; i++) {
        heap_insert(&h, &trainers[i]);
    }

    while (1) {
        t_pointer = heap_remove_min(&h);
        if(t_pointer->trainers == 'p') {
            move_pacer(trainers, t_pointer->location);
            reset_trainer_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
        }
        else if(t_pointer->trainers == 'e') {
            move_explorer(trainers, t_pointer->location);
            reset_trainer_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
        }
        else if(t_pointer->trainers == 'w') {
            move_wanderer(trainers, t_pointer->location);
            reset_trainer_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
        }
        else if(t_pointer->trainers == 'r') {
            move_rival(trainers, t_pointer->location);
            reset_trainer_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
        }
        else if(t_pointer->trainers == 'h') {
            move_hiker(trainers, t_pointer->location);
            reset_hiker_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
        }
        else if(t_pointer->trainers == '@') {
            reset_trainer_cost(trainers, t_pointer->location);
            heap_insert(&h, &trainers[t_pointer->location]);
            print_map(trainers);
            usleep(100000);
        }
    }

    heap_delete(&h);

  return 0;

}