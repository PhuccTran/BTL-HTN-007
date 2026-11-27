#ifndef STAR_MAZE_LEVELS_H
#define STAR_MAZE_LEVELS_H

#include <stdint.h>

#define STAR_MAZE_NUM_LEVELS  5U /*5 màn chơi/
#define STAR_MAZE_ROWS       12U
#define STAR_MAZE_COLS       12U
#define STAR_MAZE_CELL_PX    10U
#define STAR_MAZE_OFFSET_X   4U
#define STAR_MAZE_OFFSET_Y   25U

/* Tile IDs: 0=floor, 1=wall, 2=start, 3=goal,
   4=collectible star, 5=trap. There are no heart items. */
enum { TILE_FLOOR=0, TILE_WALL=1, TILE_START=2, TILE_GATE=3,
       TILE_STAR=4, TILE_TRAP=5 };

extern const uint8_t star_maze_levels[STAR_MAZE_NUM_LEVELS][STAR_MAZE_ROWS][STAR_MAZE_COLS];
extern const uint8_t star_maze_star_count[STAR_MAZE_NUM_LEVELS];
extern const uint8_t star_maze_trap_count[STAR_MAZE_NUM_LEVELS];

#endif
