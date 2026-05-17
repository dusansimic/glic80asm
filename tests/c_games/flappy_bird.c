#include "../common/glic80.h"

#define BIRD_X 24u
#define PIPE_WIDTH 14u
#define PIPE_GAP_PAGES 5u
#define FLAPPY_DELAY 2500u
#define FLAP_MASK (GLIC_BTN_CENTER | GLIC_BTN_UP | GLIC_BTN_A)

static unsigned char rng_state;

static const unsigned char bird_wing_up[8] = {
    0x18u, 0x3cu, 0x7eu, 0xffu, 0x7eu, 0x5au, 0x24u, 0x00u
};

static const unsigned char bird_wing_down[8] = {
    0x18u, 0x3cu, 0x7eu, 0xffu, 0x7eu, 0x24u, 0x5au, 0x00u
};

static const unsigned char bird_crash[8] = {
    0x81u, 0x42u, 0x24u, 0x18u, 0x18u, 0x24u, 0x42u, 0x81u
};

static const unsigned char pipe_texture[4] = {
    0xffu, 0xe7u, 0xdbu, 0xe7u
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

static void clear_scene(void) {
    glic_fill_graphics(GLIC_BLACK);
}

static unsigned char pipe_byte(unsigned char pipe_x,
                               unsigned char gap_page,
                               unsigned char x,
                               unsigned char page) {
    unsigned char local_x;
    unsigned char gap_end;

    if ((page >= 15u) || (x < pipe_x)) {
        return GLIC_BLACK;
    }

    local_x = (unsigned char)(x - pipe_x);
    if (local_x >= PIPE_WIDTH) {
        return GLIC_BLACK;
    }

    gap_end = (unsigned char)(gap_page + PIPE_GAP_PAGES);
    if ((page >= gap_page) && (page < gap_end)) {
        return GLIC_BLACK;
    }

    return pipe_texture[page & 3u];
}

static void restore_scene_region(unsigned char x,
                                 unsigned char width,
                                 unsigned char page,
                                 unsigned char page_count,
                                 unsigned char pipe_x,
                                 unsigned char gap_page) {
    unsigned char p;
    unsigned int offset;

    while (width != 0u) {
        if (x < GLIC_SCREEN_WIDTH) {
            p = page;
            while (p < (unsigned char)(page + page_count)) {
                offset = (((unsigned int)p) << 7) + x;
                GLIC_GVRAM[offset] = pipe_byte(pipe_x, gap_page, x, p);
                ++p;
            }
        }
        ++x;
        --width;
    }
}

static void erase_pipe(unsigned char pipe_x) {
    unsigned char i;
    unsigned char col;
    unsigned char page;

    for (i = 0u; i < PIPE_WIDTH; ++i) {
        col = (unsigned char)(pipe_x + i);
        if (col >= GLIC_SCREEN_WIDTH) {
            continue;
        }
        for (page = 0u; page < 15u; ++page) {
            GLIC_GVRAM[(((unsigned int)page) << 7) + col] = GLIC_BLACK;
        }
    }
}

static void erase_pipe_column(unsigned char x) {
    unsigned char page;

    if (x >= GLIC_SCREEN_WIDTH) {
        return;
    }
    for (page = 0u; page < 15u; ++page) {
        GLIC_GVRAM[(((unsigned int)page) << 7) + x] = GLIC_BLACK;
    }
}

static void draw_pipe_column(unsigned char pipe_x,
                             unsigned char gap_page,
                             unsigned char x) {
    unsigned char page;

    if (x >= GLIC_SCREEN_WIDTH) {
        return;
    }
    for (page = 0u; page < 15u; ++page) {
        GLIC_GVRAM[(((unsigned int)page) << 7) + x] =
            pipe_byte(pipe_x, gap_page, x, page);
    }
}

static void draw_pipe(unsigned char pipe_x, unsigned char gap_page) {
    unsigned char i;
    unsigned char page;
    unsigned char col;

    for (i = 0u; i < PIPE_WIDTH; ++i) {
        col = (unsigned char)(pipe_x + i);
        if (col >= GLIC_SCREEN_WIDTH) {
            continue;
        }
        for (page = 0u; page < 15u; ++page) {
            GLIC_GVRAM[(((unsigned int)page) << 7) + col] =
                pipe_byte(pipe_x, gap_page, col, page);
        }
    }
}

static void move_pipe_left(unsigned char old_pipe_x,
                           unsigned char pipe_x,
                           unsigned char gap_page) {
    erase_pipe_column((unsigned char)(old_pipe_x + PIPE_WIDTH - 1u));
    draw_pipe_column(pipe_x, gap_page, pipe_x);
}

static void erase_bird(unsigned char bird_y,
                       unsigned char pipe_x,
                       unsigned char gap_page) {
    unsigned char page;
    unsigned char last_page;

    page = bird_y >> 3;
    last_page = (unsigned char)((bird_y + 7u) >> 3);
    restore_scene_region(BIRD_X,
                         8u,
                         page,
                         (unsigned char)(last_page - page + 1u),
                         pipe_x,
                         gap_page);
}

static void draw_bird(unsigned char bird_y, unsigned char frame) {
    if ((frame & 4u) == 0u) {
        glic_draw_bitmap8(BIRD_X, bird_y, bird_wing_up);
    } else {
        glic_draw_bitmap8(BIRD_X, bird_y, bird_wing_down);
    }
}

static unsigned char bird_hits_pipe(unsigned char pipe_x,
                                    unsigned char gap_page,
                                    unsigned char bird_y) {
    unsigned char pipe_right;
    unsigned char bird_top_page;
    unsigned char bird_bottom_page;

    pipe_right = (unsigned char)(pipe_x + PIPE_WIDTH - 1u);
    if (((unsigned char)(BIRD_X + 7u) < pipe_x) || (BIRD_X > pipe_right)) {
        return 0u;
    }

    bird_top_page = bird_y >> 3;
    bird_bottom_page = (unsigned char)((bird_y + 7u) >> 3);
    if ((bird_top_page < gap_page) ||
        (bird_bottom_page >= (unsigned char)(gap_page + PIPE_GAP_PAGES))) {
        return 1u;
    }

    return 0u;
}

static void draw_crash(unsigned char bird_y) {
    unsigned char i;

    glic_draw_bitmap8(BIRD_X, bird_y, bird_crash);
    for (i = 0u; i < GLIC_SCREEN_WIDTH; ++i) {
        glic_plot(i, i, 1u);
        glic_plot((unsigned char)(127u - i), i, 1u);
    }
}

void main(void) {
    unsigned char buttons;
    unsigned char last_buttons;
    unsigned char pipe_x;
    unsigned char gap_page;
    unsigned char bird_y;
    unsigned char frame;
    unsigned char game_over;
    signed char velocity;
    signed int next_y;
    unsigned char old_bird_y;
    unsigned char old_pipe_x;
    unsigned char old_gap_page;

    glic_clear_text();
    rng_state = 0x5au;
    clear_scene();
    wait_for_button_press();

    while (1) {
        clear_scene();
        bird_y = 56u;
        velocity = 0;
        pipe_x = 112u;
        gap_page = 5u;
        frame = 0u;
        last_buttons = GLIC_BUTTONS_NONE;
        game_over = 0u;
        draw_pipe(pipe_x, gap_page);
        draw_bird(bird_y, frame);

        while (game_over == 0u) {
            old_bird_y = bird_y;
            old_pipe_x = pipe_x;
            old_gap_page = gap_page;
            buttons = glic_read_buttons();
            if (((last_buttons & FLAP_MASK) == FLAP_MASK) &&
                ((buttons & FLAP_MASK) != FLAP_MASK)) {
                velocity = -5;
            } else if (velocity < 4) {
                ++velocity;
            }
            last_buttons = buttons;

            next_y = (signed int)bird_y + (signed int)velocity;
            if (next_y < 0) {
                bird_y = 0u;
                game_over = 1u;
            } else if (next_y > 119) {
                bird_y = 119u;
                game_over = 1u;
            } else {
                bird_y = (unsigned char)next_y;
            }

            if (pipe_x == 0u) {
                pipe_x = 127u;
                gap_page = (unsigned char)(2u + (random8() & 7u));
                if (gap_page > 9u) {
                    gap_page = 9u;
                }
            } else {
                --pipe_x;
            }

            if (bird_hits_pipe(pipe_x, gap_page, bird_y) != 0u) {
                game_over = 1u;
            }

            erase_bird(old_bird_y, old_pipe_x, old_gap_page);
            if (pipe_x > old_pipe_x) {
                erase_pipe(old_pipe_x);
                draw_pipe(pipe_x, gap_page);
            } else {
                move_pipe_left(old_pipe_x, pipe_x, gap_page);
            }
            draw_bird(bird_y, frame);
            if (game_over != 0u) {
                draw_crash(bird_y);
            }
            ++frame;
            glic_delay(FLAPPY_DELAY);
        }

        wait_for_button_press();
    }
}
