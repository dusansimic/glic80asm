#ifndef GLIC80_H
#define GLIC80_H

#define GLIC_CVRAM ((volatile unsigned char *)0x7700)
#define GLIC_GVRAM ((volatile unsigned char *)0x7800)

#define GLIC_SCREEN_WIDTH 128u
#define GLIC_SCREEN_PAGES 16u
#define GLIC_CVRAM_BYTES 256u
#define GLIC_GVRAM_BYTES 2048u

#define GLIC_BLACK 0x00u
#define GLIC_WHITE 0xffu

#define GLIC_BUTTONS_NONE 0xffu
#define GLIC_BTN_CENTER 0x01u
#define GLIC_BTN_UP 0x02u
#define GLIC_BTN_RIGHT 0x04u
#define GLIC_BTN_DOWN 0x08u
#define GLIC_BTN_LEFT 0x10u
#define GLIC_BTN_C 0x20u
#define GLIC_BTN_B 0x40u
#define GLIC_BTN_A 0x80u

static const unsigned char glic_bit_masks[8] = {
    0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u
};

static unsigned char glic_read_buttons(void) __naked {
    __asm
        in a,(0)
        ld l,a
        ret
    __endasm;
}

static unsigned char glic_button_pressed(unsigned char buttons,
                                         unsigned char mask) {
    return (buttons & mask) == 0u;
}

static void glic_delay(unsigned int count) {
    while (count != 0u) {
        --count;
        __asm
            nop
        __endasm;
    }
}

static void glic_clear_text(void) {
    unsigned int i;

    for (i = 0; i < GLIC_CVRAM_BYTES; ++i) {
        GLIC_CVRAM[i] = 0x00u;
    }
}

static void glic_fill_graphics(unsigned char value) {
    unsigned int i;

    for (i = 0; i < GLIC_GVRAM_BYTES; ++i) {
        GLIC_GVRAM[i] = value;
    }
}

static void glic_plot(unsigned char x, unsigned char y, unsigned char value) {
    unsigned int offset;
    unsigned char mask;

    if ((x >= GLIC_SCREEN_WIDTH) || (y >= 128u)) {
        return;
    }

    offset = (((unsigned int)(y >> 3)) << 7) + x;
    mask = glic_bit_masks[y & 7u];
    if (value != 0u) {
        GLIC_GVRAM[offset] |= mask;
    } else {
        GLIC_GVRAM[offset] &= (unsigned char)~mask;
    }
}

static void glic_draw_bitmap8(unsigned char x,
                              unsigned char y,
                              const unsigned char *rows) {
    unsigned char row;
    unsigned char col;
    unsigned char bits;
    unsigned char mask;

    for (row = 0u; row < 8u; ++row) {
        bits = rows[row];
        mask = 0x80u;
        for (col = 0u; col < 8u; ++col) {
            if ((bits & mask) != 0u) {
                glic_plot((unsigned char)(x + col), (unsigned char)(y + row), 1u);
            }
            mask >>= 1;
        }
    }
}

static void glic_draw_tile8(unsigned char cell_x,
                            unsigned char cell_y,
                            const unsigned char *columns) {
    unsigned int offset;
    unsigned char col;

    offset = (((unsigned int)cell_y) << 7) + (((unsigned int)cell_x) << 3);
    for (col = 0u; col < 8u; ++col) {
        GLIC_GVRAM[offset + col] = columns[col];
    }
}

static void glic_prepare_screen(unsigned char value) {
    glic_clear_text();
    glic_fill_graphics(value);
}

static void glic_fill_page_run(unsigned char page,
                               unsigned char x,
                               unsigned char width,
                               unsigned char value) {
    volatile unsigned char *p;

    p = GLIC_GVRAM + (((unsigned int)page) << 7) + x;
    while (width != 0u) {
        *p++ = value;
        --width;
    }
}

static void glic_fill_pages(unsigned char first_page,
                            unsigned char page_count,
                            unsigned char value) {
    while (page_count != 0u) {
        glic_fill_page_run(first_page, 0u, GLIC_SCREEN_WIDTH, value);
        ++first_page;
        --page_count;
    }
}

static void glic_fill_columns(unsigned char x,
                              unsigned char width,
                              unsigned char value) {
    unsigned char page;

    for (page = 0u; page < GLIC_SCREEN_PAGES; ++page) {
        glic_fill_page_run(page, x, width, value);
    }
}

#endif
