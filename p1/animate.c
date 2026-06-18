#include "animate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum sprite_type {
    SPRITE_CIRCLE,
    SPRITE_RECTANGLE,
    SPRITE_BITMAP
};

struct sprite {
    enum sprite_type type;
    size_t count;
    
    union {
        struct {
            size_t r;
            color_t col; 
            bool fill;
        } circle;
        
        struct {
            size_t w;
            size_t h;
            color_t col; 
            bool fill;
        } rectangle;
        
        struct {
            char *f_name;
            uint32_t *pixels; 
            size_t w;
            size_t h;
        } bitmap;
    } data;
};

struct sprite_placement {
    struct sprite *obj;
    ssize_t x0,y0;
    ssize_t vx,vy;
    ssize_t ax,ay;

    struct sprite_placement *prev;
    struct sprite_placement *next;
    struct canvas *canv;

};

struct canvas {
    size_t h;
    size_t w;
    color_t bg_c;

    struct sprite_placement *head;
    struct sprite_placement *tail;
};


/* This is the header structure that we can expect to find at position 0 in a bitmap file. */
struct bitmap_header {
    uint8_t  magic[2];          // Expect {'B', 'M'}
    uint32_t size_bytes;        // Size of the file in bytes
    uint16_t reserved[2];
    uint32_t pixel_offset;      // Starting address of pixel data
// Don't pad this struct for alignment
} __attribute__((packed));

/*
 * This header immediately follows bitmap_header in the file.
 * You may ignore all fields except bV5Width and bV5Height, unless you'd like to validate
 * the image format
 */
struct bitmapv5_header {
                               // Offset     Size Description
    uint32_t bV5Size         ; // 0x00          4 Size of this header (124 bytes)
    uint32_t bV5Width        ; // 0x04          4 Width of the bitmap in pixels
    uint32_t bV5Height       ; // 0x08          4 Height of the bitmap in pixels
    uint16_t bV5Planes       ; // 0x0C          2 Number of planes (must be 1)
    uint16_t bV5BitCount     ; // 0x0E          2 Bits per pixel (e.g., 32)
    uint32_t bV5Compression  ; // 0x10          4 BI_RGB (0), BI_BITFIELDS (3)
    uint32_t bV5SizeImage    ; // 0x14          4 Size of image data (0 if uncompressed)
    uint32_t bV5XPelsPerMeter; // 0x18          4 Horizontal pixels per meter
    uint32_t bV5YPelsPerMeter; // 0x1C          4 Vertical pixels per meter
    uint32_t bV5ClrUsed      ; // 0x20          4 Number of color indices used
    uint32_t bV5ClrImportant ; // 0x24          4 Number of important colors
    uint32_t bV5RedMask      ; // 0x28          4 Color mask for red component
    uint32_t bV5GreenMask    ; // 0x2C          4 Color mask for green component
    uint32_t bV5BlueMask     ; // 0x30          4 Color mask for blue component
    uint32_t bV5AlphaMask    ; // 0x34          4 Color mask for alpha channel
    uint32_t bV5CSType       ; // 0x38          4 Color space type (e.g., LCS_CALIBRATED_RGB)
    uint8_t  bV5Endpoints[36]; // 0x3C-0x5B    36 CIE XYZ color space endpoints
    uint32_t bV5GammaRed     ; // 0x5C          4 Gamma red component
    uint32_t bV5GammaGreen   ; // 0x60          4 Gamma green component
    uint32_t bV5GammaBlue    ; // 0x64          4 Gamma blue component
    uint32_t bV5Intent       ; // 0x68          4 Rendering intent
    uint32_t bV5ProfileData  ; // 0x6C          4 Offset to ICC profile data
    uint32_t bV5ProfileSize  ; // 0x70          4 Size of embedded profile data
    uint32_t bV5Reserved     ; // 0x74          4 Reserved (must be 0)
};

void load_bmp(struct sprite *s) {

    FILE *bmp = fopen(s->data.bitmap.f_name, "rb");

    if (!bmp) {
        fprintf(stderr, "File not found");
        free(s->data.bitmap.f_name);
        s->data.bitmap.f_name = NULL;
        return;
    }

    struct bitmap_header hdr1;
    struct bitmapv5_header hdr2;

    fread(&hdr1, sizeof(struct bitmap_header), 1, bmp);
    fread(&hdr2, sizeof(struct bitmapv5_header), 1, bmp);

    if (hdr2.bV5Width == 0 || hdr2.bV5Height == 0) {
        fclose(bmp);
        return;
    }

    s->data.bitmap.w = hdr2.bV5Width;
    s->data.bitmap.h = hdr2.bV5Height;

    size_t total_pixels = s->data.bitmap.w * s->data.bitmap.h;
    s->data.bitmap.pixels = malloc(total_pixels * sizeof(uint32_t));

    if (!s->data.bitmap.pixels) {
        fclose(bmp);
        fprintf(stderr, "Malloc of pixel data failed");
        free(s->data.bitmap.f_name);
        s->data.bitmap.f_name = NULL;
        return;
    }

    fseek(bmp, hdr1.pixel_offset, SEEK_SET);

    size_t w = s->data.bitmap.w;
    size_t h = s->data.bitmap.h;

    // Row reversal of bmp contents
    for (size_t row = 0; row < h; row++) {

        size_t dest_row = h - 1 - row;
        
        uint32_t *row_destination = s->data.bitmap.pixels + (dest_row * w);

        fread(row_destination, sizeof(uint32_t), w, bmp);
    }

    fclose(bmp);
}

// --------------------Helper Functions ^----------------------------

struct canvas* animate_create_canvas(size_t height, size_t width,
                                     color_t background_color){
    struct canvas *nc = malloc(sizeof(struct canvas));
    
    if (!nc) {
        fprintf(stderr, "Error: Canvas allocation"); 
        return NULL;
    }
    
    color_t opaq_bg = (background_color & 0x00FFFFFF) | 0xFF000000;

    nc->h = height;
    nc->w = width;
    nc->bg_c = opaq_bg;
    nc->head = NULL;
    nc->tail = NULL;
    
    return nc;
}

struct sprite* animate_create_sprite(const char* file) {

    struct sprite *bmp_s = malloc(sizeof(struct sprite));

    if (!bmp_s) return NULL;

    bmp_s->count = 0;
    bmp_s->type = SPRITE_BITMAP;
    bmp_s->data.bitmap.f_name = strdup(file);

    if (!bmp_s->data.bitmap.f_name) {
        free(bmp_s); 
        fprintf(stderr, "Error: String allocation failed\n");
        return NULL;
    }

    bmp_s->data.bitmap.pixels = NULL;
    bmp_s->data.bitmap.w = 0;
    bmp_s->data.bitmap.h = 0;

    return bmp_s;
}

struct sprite* animate_create_circle(size_t radius, color_t c, bool filled) {
    
    struct sprite *circ = malloc(sizeof(struct sprite));
    if (!circ) {
        fprintf(stderr, "Error: Circle allocation");
        return NULL;
    }

    circ->type = SPRITE_CIRCLE;
    circ->count = 0;
    circ->data.circle.r = radius;
    circ->data.circle.col = c;
    circ->data.circle.fill = filled;

    return circ;
}

struct sprite* animate_create_rectangle(size_t width, size_t height,
                                        color_t c, bool filled){
    struct sprite *rec = malloc(sizeof(struct sprite));
    if (!rec) {
        fprintf(stderr, "Error: Rectangle allocation"); 
        return NULL;
    }

    color_t opaq_bg = (c & 0x00FFFFFF) | 0xFF000000;
    rec->type = SPRITE_RECTANGLE;
    rec->count = 0;
    rec->data.rectangle.w = width;
    rec->data.rectangle.h = height;
    rec->data.rectangle.col = opaq_bg;
    rec->data.rectangle.fill = filled;

    return rec;
}

bool animate_destroy_sprite(struct sprite* sprite) {

    if (sprite == NULL) return 0;

    if (sprite->count > 0) {
        return 1;
    }
    
    if (sprite->type == SPRITE_BITMAP) {
        if (sprite->data.bitmap.pixels != NULL) {
            free(sprite->data.bitmap.pixels);
        }
        if (sprite->data.bitmap.f_name != NULL) {
            free(sprite->data.bitmap.f_name);
        }
        
    }

    free(sprite);
    return 0;
}

struct sprite_placement* animate_place_sprite(struct canvas* canvas,
                                              struct sprite* sprite,
                                              ssize_t x, ssize_t y) {
    struct sprite_placement *np = malloc(sizeof(struct sprite_placement));
    
    if (!np) {
        fprintf(stderr, "Error: Placement allocation"); 
        return NULL;
    }   
    sprite->count++;

    np->canv = canvas;
    np->obj = sprite;
    np->x0 = x;
    np->y0 = y;
    np->vx = 0;
    np->vy = 0;
    np->ax = 0;
    np->ay = 0;

    np->prev = NULL;

    if (canvas->tail == NULL) {
        np->next = NULL;
        canvas->head = np;
        canvas->tail = np;
    } else {
        np->next = canvas->tail;
        canvas->tail->prev = np;
        canvas->tail = np;
    }
    
    
    return np;
}

void animate_placement_up(struct sprite_placement* sprite_placement){
    
    struct sprite_placement *np = sprite_placement;
    struct canvas *canv = np->canv;

    if (np == NULL || canv->tail == np || np->prev == NULL) {
        return;
    }

    struct sprite_placement *above = np->prev;
    struct sprite_placement *below = np->next;
    struct sprite_placement *above_above = above->prev;

    np->prev = above_above;
    if (above_above != NULL) {
        above_above->next = np;
    } else {

        canv->tail = np;
    }

    np->next = above;
    above->prev = np;

    above->next = below;
    if (below != NULL) {
        below->prev = above;
    } else {

        canv->head = above;
    }
}

void animate_placement_down(struct sprite_placement* sprite_placement){
    
    struct sprite_placement *np = sprite_placement;
    struct canvas *canv = np->canv;

    if (np == NULL || canv->head == np || np->next == NULL) {
        return;
    }

    struct sprite_placement *below = np->next;
    struct sprite_placement *above = np->prev;
    struct sprite_placement *below_below = below->next;

    below->prev = above;
    if (above != NULL) {
        above->next = below;
    } else {

        canv->tail = below;
    }

    below->next = np;
    np->prev = below;

    np->next = below_below;
    if (below_below != NULL) {
        below_below->prev = np;
    } else {

        canv->head = np;
    }
}

void animate_placement_top(struct sprite_placement* sprite_placement){
    
    struct sprite_placement *np = sprite_placement;
    struct canvas *canv = np->canv;

    if (canv->tail == np) {
        return; 
    }
    // idk why i wrote pointers backwards i should fix if i have time

    // Stitch layer BELOW (next) to point UP to the layer ABOVE it (prev)
    if (np->next != NULL) {
        np->next->prev = np->prev;
    } else {
        // If no 'next', it was the head (bottom). Update canvas head.
        canv->head = np->prev;
    }

    // Stitch layer ABOVE (prev) to point DOWN to the layer BELOW it (next)
    if (np->prev != NULL) {
        np->prev->next = np->next;
    } 
    
    // Point np DOWN at the old tail
    np->next = canv->tail;
    
    // Point np UP at nothing (it is the new top)
    np->prev = NULL; 
    
    // Point the old tail UP to np
    if (canv->tail != NULL) {
        canv->tail->prev = np; 
    }
    
    // Officially declare np as the new tail
    canv->tail = np;
}

void animate_placement_bottom(struct sprite_placement* sprite_placement){
    
    struct sprite_placement *np = sprite_placement;
    struct canvas *canv = np->canv;

    if (canv->head == np) {
        return; 
    }
    
    if (np->next != NULL) {
        np->next->prev = np->prev;
    } 

    if (np->prev != NULL) {
        np->prev->next = np->next;
    } else {

        canv->tail = np->next;
    }

    np->prev = canv->head;
    
    np->next = NULL; 

    if (canv->head != NULL) {
        canv->head->next = np; 
    }
    
    canv->head = np;
}

void animate_destroy_placement(struct sprite_placement* sprite_placement){
    
    if (sprite_placement == NULL) return;

    struct canvas *c = sprite_placement->canv;

    if (sprite_placement->obj != NULL) {
        sprite_placement->obj->count--;
    }

    if (sprite_placement->prev != NULL) {
        sprite_placement->prev->next = sprite_placement->next;
    } else {
        c->tail = sprite_placement->next;
    }

    if (sprite_placement->next != NULL) {
        sprite_placement->next->prev = sprite_placement->prev;
    } else {
        c->head = sprite_placement->prev;
    }

    free(sprite_placement);
}

void animate_set_animation_params(struct sprite_placement* sprite_placement,
                                  ssize_t vx, ssize_t vy,
                                  ssize_t ax, ssize_t ay){
                    
    if (!sprite_placement) return;

    sprite_placement->vx = vx;
    sprite_placement->vy = vy;
    sprite_placement->ax = ax;
    sprite_placement->ay = ay;
    
}

void animate_destroy_canvas(struct canvas* canvas){
    
    if (canvas == NULL) return;

    struct sprite_placement *curr = canvas->head;

    while (curr != NULL) {
        struct sprite_placement *up = curr->prev;

        animate_destroy_placement(curr);
        curr = up;
    }

    free(canvas);
}

size_t animate_frame_size_bytes(struct canvas* canvas){

    if (canvas == NULL) {
        return 0;
    }

    size_t total_pixels = canvas->w * canvas->h;
    size_t frame_size_bytes = total_pixels * sizeof(uint32_t);

    return frame_size_bytes;
}

void animate_generate_frame(const struct canvas* canvas, size_t frame,
                            size_t frame_rate, void* buf) {
    
    if (!canvas || !buf) return;

    uint32_t *pixel_buf = (uint32_t *)buf;
    size_t total_pixels = canvas->w * canvas->h;

    for (size_t i = 0; i < total_pixels; i++) {
        pixel_buf[i] = canvas->bg_c;
    }

    double t = (double)frame / (double)frame_rate;

    struct sprite_placement *curr = canvas->head;

    while (curr != NULL) {
        
        double cur_x = (double)curr->x0 + (double)curr->vx * t 
                        + 0.5 * (double)curr->ax * t * t;
        
        double cur_y = (double)curr->y0 + (double)curr->vy * t 
                        + 0.5 * (double)curr->ay * t * t;

        ssize_t draw_x = (ssize_t)round(cur_x);
        ssize_t draw_y = (ssize_t)round(cur_y);

        // Branch based on sprite type
        struct sprite *s = curr->obj;
        if (s->type == SPRITE_RECTANGLE) {
            
            size_t sw = s->data.rectangle.w;
            size_t sh = s->data.rectangle.h;
            uint32_t color = s->data.rectangle.col;

            // Loop through the sprite's dimensions
            for (size_t row = 0; row < sh; row++) {
                for (size_t col = 0; col < sw; col++) {

                    bool is_border = (row == 0 || row == sh - 1 || col == 0 ||
                                    col == sw-1);
                    
                    if (s->data.rectangle.fill || is_border) {
                        ssize_t canvas_x = draw_x + (ssize_t)col;
                        ssize_t canvas_y = draw_y + (ssize_t)row;

                        // Bounds Check: Don't draw outside the canvas memory
                        if (canvas_x >= 0 && canvas_x < (ssize_t)canvas->w &&
                            canvas_y >= 0 && canvas_y < (ssize_t)canvas->h) {
                            
                            size_t index = (canvas_y * canvas->w) + canvas_x;
                            pixel_buf[index] = color;
                    }

                    }
                }
            }
        }
        else if (s->type == SPRITE_CIRCLE) {

            size_t radius = s->data.circle.r;
            color_t color = s->data.circle.col;

            for (size_t row = 0; row <= (2*radius); row++) {
                for (size_t col = 0; col <= (2*radius); col++) {
                    
                    // Adding a filled check here would be fairly trivial
                    ssize_t x_off = col - (ssize_t)radius;
                    ssize_t y_off = row - (ssize_t)radius;

                    if ((x_off * x_off) + (y_off * y_off) <= (ssize_t)
                                                    (radius*radius)) {

                        ssize_t canvas_x = draw_x + (ssize_t)col;
                        ssize_t canvas_y = draw_y + (ssize_t)row;

                        if (canvas_x >= 0 && canvas_x < (ssize_t)canvas->w &&
                            canvas_y >= 0 && canvas_y < (ssize_t)canvas->h) {

                                size_t index = (canvas_y * canvas->w)
                                                            +canvas_x;
                                pixel_buf[index] = color;
                            }
                    }
                }
            }
        }

        else if (s->type == SPRITE_BITMAP) {

            if (s->data.bitmap.pixels == NULL && s->data.bitmap.f_name != NULL) {
                load_bmp(s);
            }

            if (s->data.bitmap.pixels == NULL) {
                curr = curr->prev;
                continue;
            }

            for (size_t row = 0; row < s->data.bitmap.h; row++) {
                for (size_t col = 0; col < s->data.bitmap.w; col++) {

                    ssize_t canvas_x = draw_x + (ssize_t)col;
                    ssize_t canvas_y = draw_y + (ssize_t)row;

                    if (canvas_x >= 0 && canvas_x < (ssize_t)canvas->w &&
                        canvas_y >= 0 && canvas_y < (ssize_t)canvas->h) {
                        
                        size_t bmp_index = (row * s->data.bitmap.w) + col;
                        uint32_t bmp_color = s->data.bitmap.pixels[bmp_index];

                        if (((bmp_color >> 24) & 0xFF) > 0) {
                            
                            size_t canvas_index = (canvas_y*canvas->w)
                                                            +canvas_x;
                            pixel_buf[canvas_index] = bmp_color | 0xFF000000;
                        }
                    }
                }
            }
        }

        // Move UP to the next layer
        curr = curr->prev; 
    }
}

// Optional extension
void animate_set_animation_function(struct sprite_placement* sprite_placement,
                                    animate_fn, void* priv) {
}

