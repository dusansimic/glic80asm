#include "../common/glic80.h"

#define SNAKE_MAX 64u
#define DIR_UP 0u
#define DIR_RIGHT 1u
#define DIR_DOWN 2u
#define DIR_LEFT 3u

static unsigned char rng_state;
static unsigned char snake_x[SNAKE_MAX];
static unsigned char snake_y[SNAKE_MAX];
static unsigned char snake_len;
static unsigned char direction;
static unsigned char fruit_x;
static unsigned char fruit_y;
static unsigned char last_tail_x;
static unsigned char last_tail_y;
static unsigned char last_head_x;
static unsigned char last_head_y;
static unsigned char last_grew;
static unsigned char last_ate_fruit;

static const unsigned char grass_a[8] = {
    0x00u, 0x10u, 0x00u, 0x01u, 0x00u, 0x40u, 0x00u, 0x04u
};

static const unsigned char grass_b[8] = {
    0x04u, 0x00u, 0x40u, 0x00u, 0x01u, 0x00u, 0x10u, 0x00u
};

static const unsigned char snake_body[8] = {
    0x3cu, 0x7eu, 0xdbu, 0xffu, 0xdbu, 0xffu, 0x7eu, 0x3cu
};

static const unsigned char snake_head[8] = {
    0x3cu, 0x7eu, 0xe7u, 0xffu, 0xffu, 0xe7u, 0x7eu, 0x3cu
};

static const unsigned char fruit_tile[8] = {
    0x18u, 0x3cu, 0x7eu, 0xfeu, 0xfeu, 0x7eu, 0x3cu, 0x18u
};

static const unsigned char border_tile[8] = {
    0xffu, 0x81u, 0xbdu, 0xa5u, 0xa5u, 0xbdu, 0x81u, 0xffu
};

static unsigned char random8(void) {
    unsigned char bit;

    bit = (unsigned char)(((rng_state >> 7) ^
                           (rng_state >> 5) ^
                           (rng_state >> 4) ^
                           (rng_state >> 3)) & 1u);
    rng_state = (unsigned char)((rng_state << 1) | bit);
    return rng_state;
}

static void wait_for_button_press(void) {
    while (glic_read_buttons() != GLIC_BUTTONS_NONE) {
        random8();
    }
    while (glic_read_buttons() == GLIC_BUTTONS_NONE) {
        random8();
    }
    while (glic_read_buttons() != GLIC_BUTTONS_NONE) {
        random8();
    }
}

static unsigned char snake_contains(unsigned char x,
                                    unsigned char y,
                                    unsigned char count) {
    unsigned char i;

    for (i = 0u; i < count; ++i) {
        if ((snake_x[i] == x) && (snake_y[i] == y)) {
            return 1u;
        }
    }
    return 0u;
}

static void place_fruit(void) {
    unsigned char tries;

    tries = 0u;
    do {
        fruit_x = random8() & 15u;
        fruit_y = (random8() >> 4) & 15u;
        ++tries;
    } while ((snake_contains(fruit_x, fruit_y, snake_len) != 0u) &&
             (tries != 0u));
}

static void reset_game(void) {
    snake_len = 4u;
    snake_x[0] = 8u;
    snake_y[0] = 8u;
    snake_x[1] = 7u;
    snake_y[1] = 8u;
    snake_x[2] = 6u;
    snake_y[2] = 8u;
    snake_x[3] = 5u;
    snake_y[3] = 8u;
    direction = DIR_RIGHT;
    place_fruit();
}

static void update_direction(unsigned char buttons) {
    if ((glic_button_pressed(buttons, GLIC_BTN_UP) != 0u) &&
        (direction != DIR_DOWN)) {
        direction = DIR_UP;
    } else if ((glic_button_pressed(buttons, GLIC_BTN_RIGHT) != 0u) &&
               (direction != DIR_LEFT)) {
        direction = DIR_RIGHT;
    } else if ((glic_button_pressed(buttons, GLIC_BTN_DOWN) != 0u) &&
               (direction != DIR_UP)) {
        direction = DIR_DOWN;
    } else if ((glic_button_pressed(buttons, GLIC_BTN_LEFT) != 0u) &&
               (direction != DIR_RIGHT)) {
        direction = DIR_LEFT;
    }
}

static void draw_floor_tile(unsigned char x, unsigned char y) {
    if (((x + y) & 1u) == 0u) {
        glic_draw_tile8(x, y, grass_a);
    } else {
        glic_draw_tile8(x, y, grass_b);
    }
}

static void draw_board(void) {
    unsigned char x;
    unsigned char y;

    for (y = 0u; y < 16u; ++y) {
        for (x = 0u; x < 16u; ++x) {
            draw_floor_tile(x, y);
        }
    }
}

static void draw_snake(void) {
    unsigned char i;

    glic_draw_tile8(fruit_x, fruit_y, fruit_tile);
    for (i = 1u; i < snake_len; ++i) {
        glic_draw_tile8(snake_x[i], snake_y[i], snake_body);
    }
    glic_draw_tile8(snake_x[0], snake_y[0], snake_head);
}

static void draw_game_over_border(void) {
    unsigned char i;

    for (i = 0u; i < 16u; ++i) {
        glic_draw_tile8(i, 0u, border_tile);
        glic_draw_tile8(i, 15u, border_tile);
        glic_draw_tile8(0u, i, border_tile);
        glic_draw_tile8(15u, i, border_tile);
    }
}

static unsigned char advance_snake(void) {
    unsigned char new_x;
    unsigned char new_y;
    unsigned char i;
    unsigned char grow;
    unsigned char collision_count;

    last_head_x = snake_x[0];
    last_head_y = snake_y[0];
    last_tail_x = snake_x[snake_len - 1u];
    last_tail_y = snake_y[snake_len - 1u];
    last_grew = 0u;
    last_ate_fruit = 0u;

    new_x = snake_x[0];
    new_y = snake_y[0];

    if (direction == DIR_UP) {
        if (new_y == 0u) {
            return 1u;
        }
        --new_y;
    } else if (direction == DIR_RIGHT) {
        if (new_x == 15u) {
            return 1u;
        }
        ++new_x;
    } else if (direction == DIR_DOWN) {
        if (new_y == 15u) {
            return 1u;
        }
        ++new_y;
    } else {
        if (new_x == 0u) {
            return 1u;
        }
        --new_x;
    }

    grow = 0u;
    if ((new_x == fruit_x) && (new_y == fruit_y)) {
        grow = 1u;
        last_ate_fruit = 1u;
    }

    collision_count = snake_len;
    if ((grow == 0u) && (collision_count != 0u)) {
        --collision_count;
    }
    if (snake_contains(new_x, new_y, collision_count) != 0u) {
        return 1u;
    }

    if ((grow != 0u) && (snake_len < SNAKE_MAX)) {
        ++snake_len;
        last_grew = 1u;
    }

    i = (unsigned char)(snake_len - 1u);
    while (i != 0u) {
        snake_x[i] = snake_x[i - 1u];
        snake_y[i] = snake_y[i - 1u];
        --i;
    }
    snake_x[0] = new_x;
    snake_y[0] = new_y;

    if (grow != 0u) {
        place_fruit();
    }

    return 0u;
}

static void draw_snake_step(void) {
    if (last_grew == 0u) {
        draw_floor_tile(last_tail_x, last_tail_y);
    }
    glic_draw_tile8(last_head_x, last_head_y, snake_body);
    glic_draw_tile8(snake_x[0], snake_y[0], snake_head);
    if (last_ate_fruit != 0u) {
        glic_draw_tile8(fruit_x, fruit_y, fruit_tile);
    }
}

static void read_controls_during_delay(void) {
    unsigned char i;

    for (i = 0u; i < 8u; ++i) {
        update_direction(glic_read_buttons());
        glic_delay(1800u);
    }
}

void main(void) {
    unsigned char dead;

    glic_clear_text();
    rng_state = 0xa7u;
    draw_board();
    wait_for_button_press();

    while (1) {
        reset_game();
        draw_board();
        draw_snake();
        dead = 0u;

        while (dead == 0u) {
            read_controls_during_delay();
            dead = advance_snake();
            if (dead == 0u) {
                draw_snake_step();
            }
        }

        draw_game_over_border();
        wait_for_button_press();
    }
}
