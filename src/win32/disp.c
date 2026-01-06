#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <windows.h>
#include "disp.h"
#include "xmon.h"
#include "options.h"

#ifdef BUILD_WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

struct image_data {
	HBITMAP hbm;
	HDC imgdc;
};


int quit;

int win_x, win_y, win_width, win_height, win_visible = 1;

struct font font;

static HINSTANCE hinst;
static HWND win;
static HDC hdc;
static unsigned int wstyle;
static int scr_bpp;

#define MAX_COLORS	236
static char cmapbuf[sizeof(LOGPALETTE) + (MAX_COLORS - 1) * sizeof(PALETTEENTRY)];
static LOGPALETTE *cmap;
static HPALETTE hpal;
static HBRUSH brush, bgbrush;
static unsigned int cur_color = 0xffffff, cur_bgcolor = 0xffffff;


int main(int argc, char **argv);

static LRESULT CALLBACK handle_event(HWND win, unsigned int msg, WPARAM wparam, LPARAM lparam);
static int parse_font(LOGFONT *lf, const char *str);


int WINAPI WinMain(HINSTANCE _hinst, HINSTANCE prev, char *cmdline, int cmdshow)
{
	char **argv, *ptr;
	int i, argc = 1;

	hinst = _hinst;

	ptr = cmdline;
	while(*ptr) {
		while(*ptr && isspace(*ptr)) ptr++;
		if(*ptr && !isspace(*ptr)) {
			argc++;
			while(*ptr && !isspace(*ptr)) ptr++;
		}
	}

	if(!(argv = malloc(argc * sizeof *argv))) {
		MessageBox(0, "Failed to allocate command line array", "Fatal", MB_OK);
		return 1;
	}
	argv[0] = "xmon";

	for(i=1; i<argc; i++) {
		while(*cmdline && isspace(*cmdline)) cmdline++;
		ptr = cmdline;
		while(*ptr && !isspace(*ptr)) ptr++;
		*ptr = 0;
		argv[i] = cmdline;
		cmdline = ptr + 1;
	}

	return main(argc, argv);
}

int init_disp(void)
{
	WNDCLASSEX wc = {0};
	TEXTMETRIC tm;
	RECT rect;
	LOGFONT lf = {0};
	HFONT hfont;

	hinst = GetModuleHandle(0);

	wc.cbSize = sizeof wc;
	wc.lpszClassName = "xmon";
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = handle_event;
	wc.hInstance = hinst;
	wc.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(101));	/* 101 is the icon, see xmon.rc */
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;

	if(!RegisterClassEx(&wc)) {
		MessageBox(0, "failed to register window class", "Fatal", MB_OK);
		return -1;
	}

	if(opt.vis.decor) {
		wstyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX;
	} else {
		wstyle = WS_POPUP;
	}

	if(!(win = CreateWindow("xmon", "xmon", wstyle, opt.x, opt.y, opt.xsz,
					opt.ysz, 0, 0, hinst, 0))) {
		MessageBox(0, "failed to create window", "Fatal", MB_OK);
		return -1;
	}
	hdc = GetDC(win);
	scr_bpp = GetDeviceCaps(hdc, BITSPIXEL) * GetDeviceCaps(hdc, PLANES);
	if(scr_bpp == 24) scr_bpp = 32;
	if(opt.verbose) {
		printf("color depth: %d bpp\n", scr_bpp);
	}

	cmap = (LOGPALETTE*)cmapbuf;
	cmap->palVersion = 0x300;

	SelectObject(hdc, GetStockObject(NULL_PEN));
	SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
	SetBkMode(hdc, TRANSPARENT);

	if(parse_font(&lf, opt.vis.font) == 0) {
		if(!(hfont = CreateFontIndirect(&lf))) {
			fprintf(stderr, "failed to create font: %s\n", opt.vis.font);
		} else {
			font.data = hfont;
			SelectObject(hdc, hfont);
		}
	}

	GetTextMetrics(hdc, &tm);
	font.height = tm.tmHeight;
	font.ascent = tm.tmAscent;
	font.descent = tm.tmDescent;

	GetClientRect(win, &rect);
	win_width = rect.right - rect.left;
	win_height = rect.bottom - rect.top;

	return 0;
}

void shutdown_disp(void)
{
	CloseWindow(win);
	UnregisterClass("xmon", hinst);
}


int proc_events(long delay)
{
	MSG msg;

	MsgWaitForMultipleObjects(0, 0, 0, delay, QS_ALLEVENTS);

	while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		if(msg.message == WM_QUIT) return -1;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

void resize_window(int x, int y)
{
	RECT rect;
	GetClientRect(win, &rect);
	rect.right = rect.left + x;
	rect.bottom = rect.top + y;
	AdjustWindowRect(&rect, wstyle, 0);
	x = rect.right - rect.left;
	y = rect.bottom - rect.top;

	SetWindowPos(win, 0, 0, 0, x, y, SWP_NOMOVE | SWP_NOOWNERZORDER |
			SWP_NOZORDER | SWP_NOACTIVATE);
}

void map_window(void)
{
	ShowWindow(win, 1);
	UpdateWindow(win);
}

unsigned int alloc_color(unsigned int r, unsigned int g, unsigned int b)
{
	unsigned int i;
	PALETTEENTRY *ent = cmap->palPalEntry;

	for(i=0; i<cmap->palNumEntries; i++) {
		if(ent->peRed == r && ent->peGreen == g && ent->peBlue == b) {
			/*printf("alloc_color(%u, %u, %u) -> %u (existing)\n", r, g, b, i);*/
			return i;
		}
		ent++;
	}

	if(cmap->palNumEntries >= MAX_COLORS) {
		fprintf(stderr, "Failed to allocate color, maximum reached\n");
		return 0;
	}
	/*printf("alloc_color(%u, %u, %u) -> %u\n", r, g, b, cmap->palNumEntries);*/
	ent->peRed = r;
	ent->peGreen = g;
	ent->peBlue = b;
	ent->peFlags = 0;
	return cmap->palNumEntries++;
}

static COLORREF get_colref(unsigned int cidx)
{
	PALETTEENTRY *col;

	if(scr_bpp <= 8) {
		return PALETTEINDEX(cidx);
	}

	col = cmap->palPalEntry + cidx;
	return RGB(col->peRed, col->peGreen, col->peBlue);
}

void set_color(unsigned int color)
{
	COLORREF colref;

	if(cur_color == color) return;

	colref = get_colref(color);

	if(brush) {
		SelectObject(hdc, GetStockObject(BLACK_BRUSH));
		DeleteObject(brush);
	}
	if((brush = CreateSolidBrush(colref))) {
		SelectObject(hdc, brush);
	}
	SetTextColor(hdc, colref);
	cur_color = color;
}

void set_background(unsigned int color)
{
	COLORREF colref;

	if(cur_bgcolor == color) return;

	colref = get_colref(color);

	if(bgbrush) {
		DeleteObject(bgbrush);
	}
	bgbrush = CreateSolidBrush(colref);
	SetBkColor(hdc, colref);
	cur_bgcolor = color;
}

void clear_window(void)
{
}

void draw_rect(int x, int y, int width, int height)
{
	Rectangle(hdc, x, y, x + width + 1, y + height + 1);
}

void draw_rects(struct rect *rects, int count)
{
	int i, x, y;
	for(i=0; i<count; i++) {
		x = rects->x;
		y = rects->y;
		Rectangle(hdc, x, y, x + rects->width + 1, y + rects->height + 1);
		rects++;
	}
}

void draw_poly(struct point *v, int count)
{
	Polygon(hdc, (POINT*)v, count);
}

void draw_text(int x, int y, const char *str)
{
	y -= font.ascent;
	TextOut(hdc, x, y, str, strlen(str));
}

void begin_drawing(void)
{
	ValidateRect(win, 0);

	if(!hpal) {
		if(!(hpal = CreatePalette(cmap))) {
			fprintf(stderr, "failed to create palette\n");
			return;
		}
	}

	SelectPalette(hdc, hpal, 0);
	RealizePalette(hdc);
}

void end_drawing(void)
{
}

struct image *create_image(unsigned int width, unsigned int height)
{
	unsigned int i, bisz, dibusage;
	struct image *img;
	struct image_data *imgdata;
	BITMAPINFO *bi;
	BITMAPINFOHEADER bih;
	unsigned short *palidx;
	BITMAP bm;

	if(!(img = calloc(1, sizeof *img + sizeof(struct image_data)))) {
		return 0;
	}
	img->data = img + 1;
	imgdata = img->data;

	img->width = width;
	img->height = height;
	img->bpp = scr_bpp;
	img->pitch = (width * img->bpp) >> 3;

	if(scr_bpp <= 8) {
		bisz = sizeof(BITMAPINFO) + cmap->palNumEntries * 2;
		bi = alloca(bisz);
		palidx = (unsigned short*)bi->bmiColors;

		memset(&bi->bmiHeader, 0, sizeof bi->bmiHeader);
		bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi->bmiHeader.biWidth = width;
		bi->bmiHeader.biHeight = -(int)height;
		bi->bmiHeader.biPlanes = 1;
		bi->bmiHeader.biBitCount = 8;
		bi->bmiHeader.biCompression = BI_RGB;	/* uncompressed */

		for(i=0; i<cmap->palNumEntries; i++) {
			palidx[i] = i;
		}
		dibusage = DIB_PAL_COLORS;
	} else {
		memset(&bih, 0, sizeof bih);
		bih.biSize = sizeof bih;
		bih.biWidth = width;
		bih.biHeight = -(int)height;
		bih.biPlanes = 1;
		bih.biBitCount = img->bpp;
		bih.biCompression = BI_RGB;

		bi = (BITMAPINFO*)&bih;

		dibusage = DIB_RGB_COLORS;

		img->rmask = 0xff0000;
		img->gmask = 0xff00;
		img->bmask = 0xff;
	}

	if(!(imgdata->hbm = CreateDIBSection(hdc, bi, dibusage, &img->pixels, 0, 0))) {
		fprintf(stderr, "failed to create DIB section\n");
		free(img);
		return 0;
	}

	GetObject(imgdata->hbm, sizeof bm, &bm);
	imgdata->imgdc = CreateCompatibleDC(hdc);
	SelectObject(imgdata->imgdc, imgdata->hbm);

	if(opt.verbose) {
		printf("image %ux%u %ubpp (pitch: %u)", img->width, img->height,
				img->bpp, img->pitch);
		if(img->bpp > 8) {
			printf(" rgbmask %x %x %x\n", img->rmask, img->gmask, img->bmask);
		} else {
			putchar('\n');
		}
	}
	return img;
}

void free_image(struct image *img)
{
	struct image_data *imgdata;

	if(!img) return;

	imgdata = img->data;
	if(imgdata->hbm) {
		DeleteObject(imgdata->hbm);
	}
	free(img);
}

void blit_image(struct image *img, int x, int y)
{
	struct image_data *imgdata = img->data;
	BitBlt(hdc, x, y, img->width, img->height, imgdata->imgdc, 0, 0, SRCCOPY);
}

void blit_subimage(struct image *img, int dx, int dy, int sx, int sy,
		unsigned int width, unsigned int height)
{
	struct image_data *imgdata = img->data;
	BitBlt(hdc, dx, dy, width, height, imgdata->imgdc, sx, sy, SRCCOPY);
}

static void global_mouse(int *x, int *y)
{
	*x += win_x;
	*y += win_y;
}

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

static LRESULT CALLBACK handle_event(HWND win, unsigned int msg, WPARAM wparam, LPARAM lparam)
{
	static int prev_mx, prev_my;

	switch(msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		begin_drawing();
		draw_window(UI_FRAME | ui_active_widgets);
		end_drawing();
		break;

	case WM_ERASEBKGND:
		if(!hpal) {
			if((hpal = CreatePalette(cmap))) {
				SelectPalette(hdc, hpal, 0);
				RealizePalette(hdc);
			}
		}
		if(bgbrush) {
			SelectObject(hdc, bgbrush);
			draw_rect(0, 0, win_width, win_height);
			return TRUE;
		}
		return DefWindowProc(win, msg, wparam, lparam);

	case WM_MOVE:
		win_x = GET_X_LPARAM(lparam);
		win_y = GET_Y_LPARAM(lparam);
		break;

	case WM_SIZE:
		if(wparam == SIZE_MINIMIZED || wparam == SIZE_MAXHIDE) {
			win_visible = 0;
		} else {
			win_visible = 1;
			win_width = LOWORD(lparam);
			win_height = HIWORD(lparam);
		}
		break;

	case WM_QUERYNEWPALETTE:
		if(!hpal) {
			if(!(hpal = CreatePalette(cmap))) {
				fprintf(stderr, "failed to create palette\n");
				return FALSE;
			}
		}
		SelectPalette(hdc, hpal, 0);
		RealizePalette(hdc);
		return TRUE;	/* we've set our own logical palette */

	case WM_KEYDOWN:
		if(wparam == 'Q' && GetKeyState(VK_CONTROL)) {
			quit = 1;
		}
		break;

	case WM_LBUTTONDOWN:
		SetCapture(win);
		prev_mx = GET_X_LPARAM(lparam);
		prev_my = GET_Y_LPARAM(lparam);
		global_mouse(&prev_mx, &prev_my);
		break;

	case WM_LBUTTONUP:
		ReleaseCapture();
		break;

	case WM_RBUTTONDOWN:
		rbutton(1, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
		break;
	case WM_RBUTTONUP:
		rbutton(0, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
		break;

	case WM_MOUSEMOVE:
		if(wparam & MK_LBUTTON) {
			int dx, dy;
			int mx = GET_X_LPARAM(lparam);
			int my = GET_Y_LPARAM(lparam);
			global_mouse(&mx, &my);
			dx = mx - prev_mx;
			dy = my - prev_my;
			prev_mx = mx;
			prev_my = my;

			SetWindowPos(win, 0, win_x + dx, win_y + dy, 0, 0, SWP_NOSIZE |
					SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE);

		} else if(wparam & MK_RBUTTON) {
			rdrag(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
		}
		break;

	default:
		return DefWindowProc(win, msg, wparam, lparam);
	}
	return 0;
}

static int parse_font(LOGFONT *lf, const char *str)
{
	char *name, *end, *tok;
	int num;

	if(!str) return -1;

	lf->lfWeight = FW_NORMAL;

	name = alloca(strlen(str) + 1);
	strcpy(name, str);

	if(!(tok = strtok(name, ":\n\r"))) {
		return -1;
	}
	strcpy(lf->lfFaceName, name);

	while((tok = strtok(0, ":\n\r"))) {
		num = strtol(tok, &end, 10);
		if(end != tok && num > 0) {
			lf->lfHeight = num;
		} else if(strcmp(tok, "bold") == 0) {
			lf->lfWeight = FW_BOLD;
		} else if(strcmp(tok, "italic") == 0) {
			lf->lfItalic = 1;
		} else {
			fprintf(stderr, "parse_font: skip unknown attribute: \"%s\"\n", tok);
		}
	}

	lf->lfQuality = PROOF_QUALITY;
	return 0;
}
