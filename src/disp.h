#ifndef DISP_H_
#define DISP_H_

struct point {
#if defined(WIN32) || defined(_WIN32)
	/* match the GDI POINT structure */
	long x, y;
#else
	/* match the X11 XPoint structure */
	short x, y;
#endif
};

struct rect {
	short x, y;
	unsigned short width, height;
};

struct color {
	int r, g, b;
};

struct image {
	unsigned int width, height, bpp;
	unsigned int pitch;
	unsigned int rmask, gmask, bmask;
	void *pixels;
	void *data;
};

struct font {
	int height;
	int ascent, descent;
	void *data;
};

extern int quit;
extern int win_x, win_y, win_width, win_height, win_visible;
extern struct font font;

int init_disp(void);
void shutdown_disp(void);

int proc_events(long delay);

void move_window(int x, int y);
void resize_window(int x, int y);
void map_window(void);

unsigned int alloc_color(unsigned int r, unsigned int g, unsigned int b);
void set_color(unsigned int color);
void set_background(unsigned int color);
void clear_window(void);
void draw_line(int x0, int y0, int x1, int y1);
void draw_rect(int x, int y, int width, int height);
void draw_rects(struct rect *rects, int count);
void draw_poly(struct point *v, int count);
void draw_text(int x, int y, const char *str);

void begin_drawing(void);
void end_drawing(void);

struct image *create_image(unsigned int width, unsigned int height);
void free_image(struct image *img);
int resize_image(struct image *img, unsigned int width, unsigned int height);
void blit_image(struct image *img, int x, int y);
void blit_subimage(struct image *img, int dx, int dy, int sx, int sy,
		unsigned int width, unsigned int height);

#endif	/* DISP_H_ */
