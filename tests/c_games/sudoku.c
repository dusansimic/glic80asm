#include "../common/glic80.h"

#define BOARD_X 1u
#define BOARD_Y 20u
#define CELL_SIZE 10u
#define BOARD_PIXELS 91u
#define LEVEL_COUNT 5u
#define TIMER_TICKS_PER_SECOND 20u
#define GAME_LOOP_DELAY 1200u
#define BLINK_DELAY 4500u
#define LIST_END 255u

static unsigned char board[81];
static unsigned char givens[81];
static unsigned char solution[81];
static unsigned char selected_level;

static const unsigned char base_solution[81] = {
    5u, 3u, 4u, 6u, 7u, 8u, 9u, 1u, 2u,
    6u, 7u, 2u, 1u, 9u, 5u, 3u, 4u, 8u,
    1u, 9u, 8u, 3u, 4u, 2u, 5u, 6u, 7u,
    8u, 5u, 9u, 7u, 6u, 1u, 4u, 2u, 3u,
    4u, 2u, 6u, 8u, 5u, 3u, 7u, 9u, 1u,
    7u, 1u, 3u, 9u, 2u, 4u, 8u, 5u, 6u,
    9u, 6u, 1u, 5u, 3u, 7u, 2u, 8u, 4u,
    2u, 8u, 7u, 4u, 1u, 9u, 6u, 3u, 5u,
    3u, 4u, 5u, 2u, 8u, 6u, 1u, 7u, 9u
};

static const unsigned char base_puzzle[81] = {
    5u, 3u, 0u, 0u, 7u, 0u, 0u, 0u, 0u,
    6u, 0u, 0u, 1u, 9u, 5u, 0u, 0u, 0u,
    0u, 9u, 8u, 0u, 0u, 0u, 0u, 6u, 0u,
    8u, 0u, 0u, 0u, 6u, 0u, 0u, 0u, 3u,
    4u, 0u, 0u, 8u, 0u, 3u, 0u, 0u, 1u,
    7u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 6u,
    0u, 6u, 0u, 0u, 0u, 0u, 2u, 8u, 0u,
    0u, 0u, 0u, 4u, 1u, 9u, 0u, 0u, 5u,
    0u, 0u, 0u, 0u, 8u, 0u, 0u, 7u, 9u
};

static const unsigned char level2_extra[] = {
    2u, 3u, 5u, 10u, 15u, 18u, 21u, 28u,
    32u, 37u, 46u, 54u, 63u, 72u, LIST_END
};

static const unsigned char level1_extra[] = {
    6u, 8u, 11u, 16u, 22u, 24u, 29u, 34u,
    38u, 43u, 47u, 52u, 56u, 60u, 65u, 70u,
    73u, 78u, LIST_END
};

static const unsigned char digit_1[7] = {
    0x04u, 0x0cu, 0x04u, 0x04u, 0x04u, 0x04u, 0x0eu
};
static const unsigned char digit_2[7] = {
    0x0eu, 0x11u, 0x01u, 0x02u, 0x04u, 0x08u, 0x1fu
};
static const unsigned char digit_3[7] = {
    0x1eu, 0x01u, 0x01u, 0x0eu, 0x01u, 0x01u, 0x1eu
};
static const unsigned char digit_4[7] = {
    0x02u, 0x06u, 0x0au, 0x12u, 0x1fu, 0x02u, 0x02u
};
static const unsigned char digit_5[7] = {
    0x1fu, 0x10u, 0x1eu, 0x01u, 0x01u, 0x11u, 0x0eu
};
static const unsigned char digit_6[7] = {
    0x06u, 0x08u, 0x10u, 0x1eu, 0x11u, 0x11u, 0x0eu
};
static const unsigned char digit_7[7] = {
    0x1fu, 0x01u, 0x02u, 0x04u, 0x08u, 0x08u, 0x08u
};
static const unsigned char digit_8[7] = {
    0x0eu, 0x11u, 0x11u, 0x0eu, 0x11u, 0x11u, 0x0eu
};
static const unsigned char digit_9[7] = {
    0x0eu, 0x11u, 0x11u, 0x0fu, 0x01u, 0x02u, 0x0cu
};

static volatile unsigned char *text_cell(unsigned char row,
                                         unsigned char col) {
    volatile unsigned char *p;

    p = GLIC_CVRAM;
    while (row != 0u) {
        p += 16u;
        --row;
    }
    p += col;
    return p;
}

static void text_put(unsigned char row, unsigned char col, unsigned char ch) {
    if ((row >= 16u) || (col >= 16u)) {
        return;
    }
    *text_cell(row, col) = ch;
}

static void text_clear_row(unsigned char row) {
    unsigned char col;

    for (col = 0u; col < 16u; ++col) {
        text_put(row, col, 0u);
    }
}

static void text_write(unsigned char row, unsigned char col, const char *text) {
    while ((*text != 0) && (col < 16u)) {
        text_put(row, col, (unsigned char)*text);
        ++text;
        ++col;
    }
}

static void draw_hline(unsigned char x,
                       unsigned char y,
                       unsigned char width,
                       unsigned char value) {
    while (width != 0u) {
        glic_plot(x, y, value);
        ++x;
        --width;
    }
}

static void draw_vline(unsigned char x,
                       unsigned char y,
                       unsigned char height,
                       unsigned char value) {
    while (height != 0u) {
        glic_plot(x, y, value);
        ++y;
        --height;
    }
}

static void draw_rect(unsigned char x,
                      unsigned char y,
                      unsigned char width,
                      unsigned char height,
                      unsigned char value) {
    draw_hline(x, y, width, value);
    draw_hline(x, (unsigned char)(y + height - 1u), width, value);
    draw_vline(x, y, height, value);
    draw_vline((unsigned char)(x + width - 1u), y, height, value);
}

static void fill_rect(unsigned char x,
                      unsigned char y,
                      unsigned char width,
                      unsigned char height,
                      unsigned char value) {
    while (height != 0u) {
        draw_hline(x, y, width, value);
        ++y;
        --height;
    }
}

static unsigned char cell_index(unsigned char row, unsigned char col) {
    unsigned char index;

    index = col;
    while (row != 0u) {
        index = (unsigned char)(index + 9u);
        --row;
    }
    return index;
}

static unsigned char cell_screen_x(unsigned char col) {
    unsigned char x;

    x = BOARD_X;
    while (col != 0u) {
        x = (unsigned char)(x + CELL_SIZE);
        --col;
    }
    return x;
}

static unsigned char cell_screen_y(unsigned char row) {
    unsigned char y;

    y = BOARD_Y;
    while (row != 0u) {
        y = (unsigned char)(y + CELL_SIZE);
        --row;
    }
    return y;
}

static unsigned char button_edge(unsigned char buttons,
                                 unsigned char last_buttons,
                                 unsigned char mask) {
    return (((last_buttons & mask) != 0u) && ((buttons & mask) == 0u));
}

static void wait_for_release(void) {
    while (glic_read_buttons() != GLIC_BUTTONS_NONE) {
        glic_delay(500u);
    }
}

static unsigned char list_contains(const unsigned char *list,
                                   unsigned char value) {
    while (*list != LIST_END) {
        if (*list == value) {
            return 1u;
        }
        ++list;
    }
    return 0u;
}

static unsigned char level_digit(unsigned char digit, unsigned char level) {
    if (digit == 0u) {
        return 0u;
    }

    if (level == 4u) {
        digit = (unsigned char)(digit + 2u);
        if (digit > 9u) {
            digit = (unsigned char)(digit - 9u);
        }
    } else if (level == 5u) {
        digit = (unsigned char)(10u - digit);
    }

    return digit;
}

static unsigned char level_source_index(unsigned char row,
                                        unsigned char col,
                                        unsigned char level) {
    if (level == 5u) {
        row = (unsigned char)(row + 3u);
        if (row >= 9u) {
            row = (unsigned char)(row - 9u);
        }
        col = (unsigned char)(col + 6u);
        if (col >= 9u) {
            col = (unsigned char)(col - 9u);
        }
    }

    return cell_index(row, col);
}

static void load_level(unsigned char level) {
    unsigned char row;
    unsigned char col;
    unsigned char dst;
    unsigned char src;
    unsigned char puzzle_value;
    unsigned char solution_value;

    dst = 0u;
    for (row = 0u; row < 9u; ++row) {
        for (col = 0u; col < 9u; ++col) {
            src = level_source_index(row, col, level);
            solution_value = level_digit(base_solution[src], level);
            puzzle_value = level_digit(base_puzzle[src], level);

            if ((level == 2u) && (puzzle_value == 0u) &&
                (list_contains(level2_extra, src) != 0u)) {
                puzzle_value = solution_value;
            } else if ((level == 1u) && (puzzle_value == 0u) &&
                       ((list_contains(level2_extra, src) != 0u) ||
                        (list_contains(level1_extra, src) != 0u))) {
                puzzle_value = solution_value;
            }

            board[dst] = puzzle_value;
            solution[dst] = solution_value;
            givens[dst] = (puzzle_value == 0u) ? 0u : 1u;
            ++dst;
        }
    }
}

static const unsigned char *digit_rows(unsigned char digit) {
    if (digit == 1u) {
        return digit_1;
    }
    if (digit == 2u) {
        return digit_2;
    }
    if (digit == 3u) {
        return digit_3;
    }
    if (digit == 4u) {
        return digit_4;
    }
    if (digit == 5u) {
        return digit_5;
    }
    if (digit == 6u) {
        return digit_6;
    }
    if (digit == 7u) {
        return digit_7;
    }
    if (digit == 8u) {
        return digit_8;
    }
    return digit_9;
}

static void draw_digit(unsigned char x, unsigned char y, unsigned char digit) {
    const unsigned char *rows;
    unsigned char row;
    unsigned char col;
    unsigned char bits;
    unsigned char mask;

    rows = digit_rows(digit);
    for (row = 0u; row < 7u; ++row) {
        bits = rows[row];
        mask = 0x10u;
        for (col = 0u; col < 5u; ++col) {
            if ((bits & mask) != 0u) {
                glic_plot((unsigned char)(x + col),
                          (unsigned char)(y + row),
                          1u);
            }
            mask >>= 1;
        }
    }
}

static void draw_grid(void) {
    unsigned char i;
    unsigned char x;
    unsigned char y;
    unsigned char thick;

    x = BOARD_X;
    for (i = 0u; i < 10u; ++i) {
        thick = ((i == 0u) || (i == 3u) || (i == 6u) || (i == 9u));
        draw_vline(x, BOARD_Y, BOARD_PIXELS, 1u);
        if (thick != 0u) {
            draw_vline((unsigned char)(x + 1u), BOARD_Y, BOARD_PIXELS, 1u);
        }
        x = (unsigned char)(x + CELL_SIZE);
    }

    y = BOARD_Y;
    for (i = 0u; i < 10u; ++i) {
        thick = ((i == 0u) || (i == 3u) || (i == 6u) || (i == 9u));
        draw_hline(BOARD_X, y, BOARD_PIXELS, 1u);
        if (thick != 0u) {
            draw_hline(BOARD_X, (unsigned char)(y + 1u), BOARD_PIXELS, 1u);
        }
        y = (unsigned char)(y + CELL_SIZE);
    }
}

static void clear_cell(unsigned char row, unsigned char col) {
    fill_rect((unsigned char)(cell_screen_x(col) + 2u),
              (unsigned char)(cell_screen_y(row) + 2u),
              7u,
              7u,
              0u);
}

static void draw_cell(unsigned char row, unsigned char col, unsigned char index) {
    unsigned char value;

    clear_cell(row, col);
    value = board[index];
    if (value != 0u) {
        draw_digit((unsigned char)(cell_screen_x(col) + 3u),
                   (unsigned char)(cell_screen_y(row) + 2u),
                   value);
    }
}

static void draw_cursor(unsigned char row, unsigned char col) {
    draw_rect((unsigned char)(cell_screen_x(col) + 2u),
              (unsigned char)(cell_screen_y(row) + 2u),
              7u,
              7u,
              1u);
}

static void draw_board_values(void) {
    unsigned char row;
    unsigned char col;
    unsigned char index;

    index = 0u;
    for (row = 0u; row < 9u; ++row) {
        for (col = 0u; col < 9u; ++col) {
            draw_cell(row, col, index);
            ++index;
        }
    }
}

static unsigned char tens_digit(unsigned char value) {
    unsigned char tens;

    tens = 0u;
    while (value >= 10u) {
        value = (unsigned char)(value - 10u);
        ++tens;
    }
    return tens;
}

static unsigned char ones_digit(unsigned char value) {
    while (value >= 10u) {
        value = (unsigned char)(value - 10u);
    }
    return value;
}

static void write_two_digits(unsigned char row,
                             unsigned char col,
                             unsigned char value) {
    text_put(row, col, (unsigned char)('0' + tens_digit(value)));
    text_put(row, (unsigned char)(col + 1u),
             (unsigned char)('0' + ones_digit(value)));
}

static void draw_game_text(unsigned char level,
                           unsigned char selected_digit,
                           unsigned char minutes,
                           unsigned char seconds) {
    text_clear_row(0u);
    text_write(0u, 0u, "L1 T00:00 N1");
    text_put(0u, 1u, (unsigned char)('0' + level));
    text_put(0u, 11u, (unsigned char)('0' + selected_digit));
    write_two_digits(0u, 4u, minutes);
    write_two_digits(0u, 7u, seconds);

    text_clear_row(14u);
    text_clear_row(15u);
    text_write(14u, 0u, "MOVE CTR PLACE");
    text_write(15u, 0u, "A/B NUM C MENU");
}

static void update_timer_text(unsigned char minutes, unsigned char seconds) {
    write_two_digits(0u, 4u, minutes);
    write_two_digits(0u, 7u, seconds);
}

static void update_selected_digit_text(unsigned char selected_digit) {
    text_put(0u, 11u, (unsigned char)('0' + selected_digit));
}

static void draw_game_screen(unsigned char level,
                             unsigned char selected_digit,
                             unsigned char minutes,
                             unsigned char seconds,
                             unsigned char cursor_row,
                             unsigned char cursor_col) {
    glic_prepare_screen(GLIC_BLACK);
    draw_game_text(level, selected_digit, minutes, seconds);
    draw_grid();
    draw_board_values();
    draw_cursor(cursor_row, cursor_col);
}

static void blink_cell(unsigned char row,
                       unsigned char col,
                       unsigned char index) {
    unsigned char i;

    for (i = 0u; i < 6u; ++i) {
        fill_rect((unsigned char)(cell_screen_x(col) + 2u),
                  (unsigned char)(cell_screen_y(row) + 2u),
                  7u,
                  7u,
                  (i & 1u) ? 0u : 1u);
        if ((i & 1u) != 0u) {
            draw_cell(row, col, index);
            draw_cursor(row, col);
        }
        glic_delay(BLINK_DELAY);
    }
    draw_cell(row, col, index);
    draw_cursor(row, col);
}

static unsigned char puzzle_solved(void) {
    unsigned char i;

    for (i = 0u; i < 81u; ++i) {
        if (board[i] != solution[i]) {
            return 0u;
        }
    }
    return 1u;
}

static void show_solved_screen(void) {
    unsigned char buttons;

    text_clear_row(14u);
    text_clear_row(15u);
    text_write(14u, 4u, "SOLVED");
    text_write(15u, 2u, "CENTER MENU");
    wait_for_release();

    while (1) {
        buttons = glic_read_buttons();
        if ((glic_button_pressed(buttons, GLIC_BTN_CENTER) != 0u) ||
            (glic_button_pressed(buttons, GLIC_BTN_C) != 0u)) {
            wait_for_release();
            return;
        }
        glic_delay(GAME_LOOP_DELAY);
    }
}

static void draw_title_level(unsigned char level) {
    text_clear_row(5u);
    text_write(5u, 4u, "LEVEL 1");
    text_put(5u, 10u, (unsigned char)('0' + level));
}

static void draw_title_screen(unsigned char level) {
    glic_prepare_screen(GLIC_BLACK);
    draw_rect(88u, 8u, 36u, 36u, 1u);
    draw_vline(100u, 8u, 36u, 1u);
    draw_vline(112u, 8u, 36u, 1u);
    draw_hline(88u, 20u, 36u, 1u);
    draw_hline(88u, 32u, 36u, 1u);

    text_write(1u, 4u, "SUDOKU");
    draw_title_level(level);
    text_write(7u, 2u, "< > CHANGE LVL");
    text_write(9u, 2u, "CENTER START");
    text_write(11u, 2u, "C CONTROLS");
}

static void show_controls_screen(void) {
    unsigned char buttons;

    glic_prepare_screen(GLIC_BLACK);
    text_write(1u, 2u, "SUDOKU KEYS");
    text_write(3u, 1u, "ARROWS MOVE");
    text_write(4u, 1u, "A/B PICK NUM");
    text_write(5u, 1u, "CENTER PLACE");
    text_write(6u, 1u, "C BACK MENU");
    text_write(8u, 1u, "TITLE SCREEN");
    text_write(9u, 1u, "< > LEVEL");
    text_write(10u, 1u, "CENTER START");
    text_write(12u, 3u, "PRESS C");
    wait_for_release();

    while (1) {
        buttons = glic_read_buttons();
        if ((glic_button_pressed(buttons, GLIC_BTN_C) != 0u) ||
            (glic_button_pressed(buttons, GLIC_BTN_CENTER) != 0u)) {
            wait_for_release();
            return;
        }
        glic_delay(GAME_LOOP_DELAY);
    }
}

static unsigned char title_menu(void) {
    unsigned char buttons;
    unsigned char last_buttons;

    draw_title_screen(selected_level);
    last_buttons = GLIC_BUTTONS_NONE;

    while (1) {
        buttons = glic_read_buttons();
        if (button_edge(buttons, last_buttons, GLIC_BTN_RIGHT) != 0u) {
            if (selected_level < LEVEL_COUNT) {
                ++selected_level;
            } else {
                selected_level = 1u;
            }
            draw_title_level(selected_level);
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_LEFT) != 0u) {
            if (selected_level > 1u) {
                --selected_level;
            } else {
                selected_level = LEVEL_COUNT;
            }
            draw_title_level(selected_level);
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_C) != 0u) {
            show_controls_screen();
            draw_title_screen(selected_level);
            last_buttons = GLIC_BUTTONS_NONE;
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_CENTER) != 0u) {
            wait_for_release();
            return selected_level;
        }

        last_buttons = buttons;
        glic_delay(GAME_LOOP_DELAY);
    }
}

static void run_game(unsigned char level) {
    unsigned char cursor_row;
    unsigned char cursor_col;
    unsigned char selected_digit;
    unsigned char minutes;
    unsigned char seconds;
    unsigned char ticks;
    unsigned char buttons;
    unsigned char last_buttons;
    unsigned char old_row;
    unsigned char old_col;
    unsigned char index;

    load_level(level);
    cursor_row = 0u;
    cursor_col = 0u;
    selected_digit = 1u;
    minutes = 0u;
    seconds = 0u;
    ticks = 0u;
    last_buttons = GLIC_BUTTONS_NONE;

    draw_game_screen(level, selected_digit, minutes, seconds, cursor_row, cursor_col);

    while (1) {
        buttons = glic_read_buttons();
        old_row = cursor_row;
        old_col = cursor_col;

        if ((button_edge(buttons, last_buttons, GLIC_BTN_UP) != 0u) &&
            (cursor_row > 0u)) {
            --cursor_row;
        } else if ((button_edge(buttons, last_buttons, GLIC_BTN_DOWN) != 0u) &&
                   (cursor_row < 8u)) {
            ++cursor_row;
        } else if ((button_edge(buttons, last_buttons, GLIC_BTN_LEFT) != 0u) &&
                   (cursor_col > 0u)) {
            --cursor_col;
        } else if ((button_edge(buttons, last_buttons, GLIC_BTN_RIGHT) != 0u) &&
                   (cursor_col < 8u)) {
            ++cursor_col;
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_A) != 0u) {
            if (selected_digit < 9u) {
                ++selected_digit;
            } else {
                selected_digit = 1u;
            }
            update_selected_digit_text(selected_digit);
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_B) != 0u) {
            if (selected_digit > 1u) {
                --selected_digit;
            } else {
                selected_digit = 9u;
            }
            update_selected_digit_text(selected_digit);
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_C) != 0u) {
            wait_for_release();
            return;
        } else if (button_edge(buttons, last_buttons, GLIC_BTN_CENTER) != 0u) {
            index = cell_index(cursor_row, cursor_col);
            if ((givens[index] != 0u) || (selected_digit != solution[index])) {
                blink_cell(cursor_row, cursor_col, index);
            } else {
                board[index] = selected_digit;
                draw_cell(cursor_row, cursor_col, index);
                draw_cursor(cursor_row, cursor_col);
                if (puzzle_solved() != 0u) {
                    show_solved_screen();
                    return;
                }
            }
        }

        if ((old_row != cursor_row) || (old_col != cursor_col)) {
            index = cell_index(old_row, old_col);
            draw_cell(old_row, old_col, index);
            draw_cursor(cursor_row, cursor_col);
        }

        last_buttons = buttons;
        ++ticks;
        if (ticks >= TIMER_TICKS_PER_SECOND) {
            ticks = 0u;
            ++seconds;
            if (seconds >= 60u) {
                seconds = 0u;
                if (minutes < 99u) {
                    ++minutes;
                }
            }
            update_timer_text(minutes, seconds);
        }
        glic_delay(GAME_LOOP_DELAY);
    }
}

void main(void) {
    unsigned char level;

    selected_level = 1u;
    while (1) {
        level = title_menu();
        run_game(level);
    }
}
