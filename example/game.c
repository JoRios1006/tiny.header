#include "../tiny.h"
// Simple terminal-based example using tiny.h
// Demonstrates input handling, rendering, and basic state management
#define FRAME_MS 33
#define MAX_ROWS 20
#define MAX_COLS 20
typedef struct {
  int x, y;
  int is_running;
} Game;

static inline void render(Game *game) {
  CLEAR_SCREEN();
  MOVE_CURSOR(game->y, game->x);
  PRINT_STR("@", 1);
  MOVE_CURSOR(1, 1);
  PRINT_STR("wasd=move  q=quit", 17);
}
static void move_up(Game *game) { game->y = CLAMP(game->y - 1, 1, MAX_ROWS); }
static void move_down(Game *game) { game->y = CLAMP(game->y + 1, 1, MAX_ROWS); }
static void move_left(Game *game) { game->x = CLAMP(game->x - 1, 1, MAX_COLS); }
static void move_right(Game *game) { game->x = CLAMP(game->x + 1, 1, MAX_COLS); }
static void quit_game(Game *game) { game->is_running = 0; }
static void do_nothing(Game *game) { (void)game; }
typedef void (*KeyAction)(Game *);

KeyAction KEYMAP[256] = {[0 ... 255] = do_nothing};
static inline void init_input() {
  KEYMAP['w'] = move_up;
  KEYMAP['s'] = move_down;
  KEYMAP['a'] = move_left;
  KEYMAP['d'] = move_right;
  KEYMAP['q'] = quit_game;
  KEYMAP[KEY_UP] = move_up;
  KEYMAP[KEY_DOWN] = move_down;
  KEYMAP[KEY_LEFT] = move_left;
  KEYMAP[KEY_RIGHT] = move_right;
}
static inline void handle_input(Game *game) {
  if (POLL_KEY(FRAME_MS)) {
    int k = READ_KEY();
    if (k == KEY_ESC)
      k = CONSUME_ESCAPE();
    if (k >= 0 && k < 256)
      KEYMAP[k](game);
  }
}

BEGIN
TERM_RAW();
TERM_NONBLOCK();
CURSOR_HIDE();
Game game = {.x = 10, .y = 10, .is_running = 1};
init_input();
GAME_LOOP :;
render(&game);
handle_input(&game);
if (game.is_running)
  goto GAME_LOOP;
CURSOR_SHOW();
TERM_RESET();
CLEAR_SCREEN();
END
