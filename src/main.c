#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <sanitizer/lsan_interface.h>

#include "../inc/SDL2/include/SDL.h"
#include "../inc/lalg.h"

static const uint32_t SCREEN_WIDTH  = 800;
static const uint32_t SCREEN_HEIGHT = 800;
static const uint32_t PIXELS_NUMBER = SCREEN_WIDTH*SCREEN_HEIGHT;

//static const uint32_t RED   = 0x00FF0000;
//static const uint32_t GREEN = 0x0000FF00;
static const uint32_t BLUE  = 0x000000FF;

typedef struct {
	SDL_Window*  window;
	SDL_Surface* surface;
	SDL_Event    event;
	size_t       bytes_per_pixel;
} SDLContext;

typedef struct {
	uint32_t flags;
	bool grid_on;
} State;

State state = {
	.flags = 0,
	.grid_on = true
};

typedef struct {
        V3f position;
	V3f forward;
        V3f up;
        float fovy;
	float znear;
	float zfar;
} Camera;

void pixel_set(uint32_t x, uint32_t y, uint32_t* buffer, uint32_t color) {

	buffer[x + y*SCREEN_WIDTH] = color;
}

static inline Camera camera_default(void) {

	Camera c = {
		.position = (V3f) {{ 4.0f, 1.0f, -12.0f }},
		.forward  = (V3f) {{ 0.0f, 0.0f,   1.0f }},
		.up 	  = (V3f) {{ 0.0f, 1.0f,   0.0f }},
		.fovy     = 120.0f,
		.znear    = 1.0f,
		.zfar     = 100.0f
	};

	return c;
}

void memory_free(SDLContext* ctx) {

	SDL_DestroyWindow(ctx->window);
	free(ctx);
}

void context_sdl_init(SDLContext* ctx) {

	// suppress SDL memory leak errors
	__lsan_disable();

	SDL_Init(SDL_INIT_VIDEO);
	ctx->window = SDL_CreateWindow("SoftRend", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
						   SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!ctx->window) fprintf(stderr, "Failed to create window. Error: %s\n", SDL_GetError());

	ctx->surface = SDL_GetWindowSurface(ctx->window);
	ctx->bytes_per_pixel = ctx->surface->format->BytesPerPixel;


	__lsan_enable();
}

static inline V3f world_to_view(V3f v_in, Camera c) {

	V3f res   = {0};
	V3f right = norm_3f(cross_3f(c.up, c.forward));

	V3f rel = {
		.x = v_in.x - c.position.x,
		.y = v_in.y - c.position.y,
		.z = v_in.z - c.position.z
	};

        res.x = rel.x * right.x     + rel.y * right.y     + rel.z * right.z;
        res.y = rel.x * c.up.x      + rel.y * c.up.y      + rel.z * c.up.z;
        res.z = rel.x * c.forward.x + rel.y * c.forward.y + rel.z * c.forward.z;

	return res;
}

V2s get_image_crd(V3f v, Camera camera) {

	const float a = (float) SCREEN_HEIGHT / (float) SCREEN_WIDTH;
	const float f = 1 / tanf(0.5f * camera.fovy * M_PI / 180.0f);
	//const float zfar = camera.zfar;
	//const float znear = camera.znear;
	//const float l = zfar / (zfar - znear) - zfar*znear / (zfar-znear);

	float px = a * f * v.x;
	float py = f * v.y;
	//float pz = l*v.z - l*znear;

	// perspective divide
	if (v.z*v.z > 0.0) {
		px /= v.z;
		py /= v.z;
	}

	int32_t x_screen = (int32_t)((px + 1.0f) * 0.5f * SCREEN_WIDTH);
	int32_t y_screen = (int32_t)((1.0f - py) * 0.5f * SCREEN_HEIGHT);

	return (V2s){ x_screen, y_screen };
}

// cohen sutherland, see wikipedia
bool clipline(int* x1,int* y1,int* x2,int* y2) {

	static const int CLIPLEFT  = 1;
	static const int CLIPRIGHT = 2;
	static const int CLIPLOWER = 4;
	static const int CLIPUPPER = 8;
	static const int XMin = 10;
	static const int YMin = 10;
	static const int XMax = SCREEN_WIDTH  - 10;
	static const int YMax = SCREEN_HEIGHT - 10;

	int K1 = 0;
	int K2 = 0;
	int dx = *x2 - *x1;
	int dy = *y2 - *y1;

	if(*y1 < YMin) K1  = CLIPLOWER;
	if(*y1 > YMax) K1  = CLIPUPPER;
	if(*x1 < XMin) K1 |= CLIPLEFT;
	if(*x1 > XMax) K1 |= CLIPRIGHT;

	if(*y2 < YMin) K2  = CLIPLOWER;
	if(*y2 > YMax) K2  = CLIPUPPER;
	if(*x2 < XMin) K2 |= CLIPLEFT;
	if(*x2 > XMax) K2 |= CLIPRIGHT;

	while(K1 || K2) {
    		if(K1 & K2) return false;
		if(K1) {
			if(K1 & CLIPLEFT) {
				*y1 += (XMin - *x1) * dy / dx;
				*x1  = XMin;
			}
			else if(K1 & CLIPRIGHT) {
				*y1 += (XMax - *x1) * dy / dx;
				*x1  = XMax;
			}

			if(K1 & CLIPLOWER) {
				*x1 += (YMin - *y1) * dx / dy;
				*y1  = YMin;
			}
			else if(K1 & CLIPUPPER) {
				*x1 += (YMax - *y1) * dx / dy;
				*y1  = YMax;
			}
			K1 = 0;
			if(*y1 < YMin) K1  = CLIPLOWER;
			if(*y1 > YMax) K1  = CLIPUPPER;
			if(*x1 < XMin) K1 |= CLIPLEFT;
			if(*x1 > XMax) K1 |= CLIPRIGHT;
    		}

    		if(K1 & K2) return false;

		if(K2) {
			if(K2 & CLIPLEFT) {
				*y2 += (XMin - *x2) * dy / dx;
				*x2  = XMin;
			} else if(K2 & CLIPRIGHT) {
				*y2 += (XMax - *x2) * dy / dx;
				*x2  = XMax;
			}
			if(K2 & CLIPLOWER) {
				*x2 += (YMin - *y2) * dx / dy;
				*y2  = YMin;
			}
			else if(K2 & CLIPUPPER) {
				*x2 += (YMax - *y2) * dx / dy;
				*y2 = YMax;
			}
			K2 = 0;

			if(*y2 < YMin) K2  = CLIPLOWER;
			if(*y2 > YMax) K2  = CLIPUPPER;
			if(*x2 < XMin) K2 |= CLIPLEFT;
			if(*x2 > XMax) K2 |= CLIPRIGHT;
		}
	}
  	return true;
}

void line_draw(V3f p1, V3f p2, uint32_t* buffer, uint32_t color, Camera camera) {

	// change to cam basis for near-plane clipping
	p1 = world_to_view(p1, camera);
	p2 = world_to_view(p2, camera);

	if (p1.z <= camera.znear && p2.z <= camera.znear) return;

	// this implies p1.z > c.znear
	if (p1.z < camera.znear) {
		// get new p0 behind clipping plane
		p1 = intersect_z(p2, p1, camera.znear);
	}

	// this implies p0.z > c.znear
	if (p2.z < camera.znear) {
		p2 = intersect_z(p1, p2, camera.znear);
	}

	V2s start = get_image_crd(p1, camera);
	V2s end   = get_image_crd(p2, camera);

	if (!clipline(&start.x, &start.y, &end.x, &end.y)) {
		return;
	}

	int dx =  abs((int)end.x - (int)start.x);
	int sx = (int)start.x < (int)end.x ? 1 : -1;

	int dy = -abs((int)end.y - (int)start.y);
	int sy = (int)start.y < (int)end.y ? 1 : -1;

	int err = dx + dy;
	while (1) {
		pixel_set(start.x, start.y, buffer, color);
		if (start.x == end.x && start.y == end.y) break;
		int e2 = 2 * err;
		if (e2 > dy) { err += dy; start.x += sx; }
		if (e2 < dx) { err += dx; start.y += sy; }
	}
}

void grid_draw(uint32_t* buffer, Camera camera) {

	int32_t grid_const = 40;
	uint32_t color = BLUE;

	for (int32_t i = -grid_const; i <= grid_const; i+=1) {
		V3f p1 = { .x = (float) i, .y = 0.0f, .z = -((float) grid_const) };
		V3f p2 = { .x = (float) i, .y = 0.0f, .z = +((float) grid_const) };
		line_draw(p1, p2, buffer, color, camera);
	}

	for (int32_t i = -grid_const; i <= grid_const; i+=1) {
		V3f p1 = { .x = -((float) grid_const), .y = 0.0f, .z = (float) i};
		V3f p2 = { .x = +((float) grid_const), .y = 0.0f, .z = (float) i};
		line_draw(p1, p2, buffer, color, camera);
	}
}

void buffer_flush(uint32_t* buffer, uint8_t bytes_per_pixel) {

	memset(buffer, 0, PIXELS_NUMBER * bytes_per_pixel);
}

void event_loop(SDLContext* ctx, uint32_t* buffer, Camera camera) {

	// for fps calculation
	struct timespec t0 = {0};
	struct timespec t1 = {0};
	size_t step = 0;

	bool running = true;
	while (running) {

		// start time measuring
		if (step % 100 == 0) {
			t0 = (struct timespec){0};
			t1 = (struct timespec){0};
			timespec_get(&t0, TIME_UTC);
		}

		while (SDL_PollEvent(&ctx->event) != 0) {

			switch (ctx->event.type) {

			case SDL_KEYDOWN:
				if (ctx->event.key.keysym.sym == SDLK_ESCAPE) running = false;
				if (ctx->event.key.keysym.sym == SDLK_g) state.grid_on = !state.grid_on;

				break;

			case SDL_MOUSEMOTION:
				V3f rel = {
					.x = ctx->event.motion.xrel,
					.y = ctx->event.motion.yrel,
					.z = 0
				};
				camera.position = sub_3f(camera.position,rel);

			default:
				break;
			}
		}
		if (state.grid_on) grid_draw(buffer, camera);

		SDL_UpdateWindowSurface(ctx->window);
		buffer_flush(buffer, ctx->bytes_per_pixel);

		// end time measuring
    		timespec_get(&t1, TIME_UTC);
    		long dns = t1.tv_nsec - t0.tv_nsec;
		double frame_time_ms = dns / (1000.0*1000.0);
		double frame_time_s  = frame_time_ms / 1000.0;
		if (step % 100 == 0) {
    			printf("frame time = %.2f ms\n", frame_time_ms);
    			printf("       FPS = %.2f\n", 1/frame_time_s);
			step = 0;
		}
		step += 1;
	}
}

int main(void) {

	SDLContext* ctx = calloc((size_t) 1, (size_t) sizeof *ctx);

	context_sdl_init(ctx);

	uint32_t* buffer = ctx->surface->pixels;

	Camera camera = camera_default();

	event_loop(ctx, buffer, camera);

	memory_free(ctx);
	SDL_Quit();

	return 0;
}

