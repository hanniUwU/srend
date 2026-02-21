#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <sanitizer/lsan_interface.h>

#include "../inc/SDL2/include/SDL.h"
#include "../inc/lalg.h"
#include "../inc/camera.h"
#include "../inc/text.h"
#include "../inc/color.h"
#include "../assets/asset_cube.h"
#include "../assets/asset_teapot.h"

static         size_t lines_count_global    = 0;
static         size_t triangle_count_global = 0;

static const uint32_t SCREEN_WIDTH  = 1920;
static const uint32_t SCREEN_HEIGHT = 1080;
static const uint32_t PIXELS_NUMBER = SCREEN_WIDTH*SCREEN_HEIGHT;

typedef struct {
	SDL_Window*  window;
	SDL_Surface* surface;
	SDL_Event    event;
	size_t       bytes_per_pixel;
} SDLContext;

typedef struct {
	uint32_t flags;
	bool grid_on;
	bool wireframe;
} State;

State state = {
	.flags = 0,
	.grid_on = true,
	.wireframe = true
};

void pixel_set(uint32_t x, uint32_t y, uint32_t* buffer, uint32_t color)
{

if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;


	//printf("here! ps\n");
    buffer[y * SCREEN_WIDTH + x] = color;
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


	SDL_SetRelativeMouseMode(SDL_TRUE);
	__lsan_enable();
}

static inline V3f world_to_view(V3f v_in, Camera c) {

	V3f res   = {0};
	V3f right = norm_3f(cross_3f(c.forward, c.up));

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

	float px = a * f * v.x;
	float py = f * v.y;
	float pz = fmaxf(v.z, camera.znear);

	// perspective divide
	px /= pz;
	py /= pz;

	int32_t x_screen = (int32_t)((px + 1.0f) * 0.5f * (int32_t)SCREEN_WIDTH);
	int32_t y_screen = (int32_t)((1.0f - py) * 0.5f * (int32_t)SCREEN_HEIGHT);
	// Clamp to avoid overflow of small V2s types
	if (x_screen < INT32_MIN) x_screen = INT32_MIN;
	if (x_screen > INT32_MAX) x_screen = INT32_MAX;
	if (y_screen < INT32_MIN) y_screen = INT32_MIN;
	if (y_screen > INT32_MAX) y_screen = INT32_MAX;

	return (V2s){ x_screen, y_screen };
}

#define XMIN 40
#define YMIN 40
#define XMAX (SCREEN_WIDTH - 40)
#define YMAX (SCREEN_HEIGHT - 40)

static const float EPS = 1e-12;

// Liang–Barsky clipping.
bool clipline(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2) {

	const float dx = (float) (*x2 - *x1);
	const float dy = (float) (*y2 - *y1);

	float p[4] = { -dx, dx, -dy, dy };
	float q[4] = { *x1 - (float) XMIN, (float) XMAX - *x1,
		       *y1 - (float) YMIN, (float) YMAX - *y1 };

	float u1 = 0.0f;
	float u2 = 1.0f;

	for (int i = 0; i < 4; ++i) {
		float pi = p[i];
		float qi = q[i];

		/* Handle near-parallel (pi == 0) robustly */
		if (fabs(pi) < EPS) {
			if (qi < 0.0f) return false;
		} else {
			float r = qi / pi;
	                if (pi < 0.0f) {
                		if (r > u1) u1 = r;
			} else {
		                if (r < u2) u2 = r;
           		}
			if (u1 > u2) return false;
            	}
	}

	/* compute clipped coordinates using original x1,y1,d */
	float nx1 = *x1 + u1 * dx;
	float ny1 = *y1 + u1 * dy;
	float nx2 = *x1 + u2 * dx;
	float ny2 = *y1 + u2 * dy;

	/* round to nearest integer (llround available in math.h) */
	*x1 = (int32_t) llroundf(nx1);
	*y1 = (int32_t) llroundf(ny1);
	*x2 = (int32_t) llroundf(nx2);
	*y2 = (int32_t) llroundf(ny2);

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

	if ( start.x < (int32_t) XMIN && end.x < (int32_t) XMIN) return;
	if ( start.y < (int32_t) YMIN && end.y < (int32_t) YMIN) return;
	if ( start.x > (int32_t) XMAX && end.x > (int32_t) XMAX) return;
	if ( start.y > (int32_t) YMAX && end.y > (int32_t) YMAX) return;

	if (!clipline(&start.x, &start.y, &end.x, &end.y)) {
		return;
	}
	lines_count_global += 1;

	int32_t dx =  abs((int32_t)end.x - (int32_t)start.x);
	int32_t sx = (int32_t)start.x < (int32_t)end.x ? 1 : -1;

	int32_t dy = -abs((int32_t)end.y - (int32_t)start.y);
	int32_t sy = (int32_t)start.y < (int32_t)end.y ? 1 : -1;

	int32_t err = dx + dy;
	while (1) {
		pixel_set(start.x, start.y, buffer, color);
		if (start.x == end.x && start.y == end.y) break;
		int32_t e2 = 2 * err;
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

bool is_point_in_triangle(V2u p, Triangle t) {

	V3f v1 = t.v1;
	V3f v2 = t.v2;
	V3f v3 = t.v3;

	V2s a = { .x = (int32_t)v1.x, .y = (int32_t)v1.y };
	V2s b = { .x = (int32_t)v2.x, .y = (int32_t)v2.y };
	V2s c = { .x = (int32_t)v3.x, .y = (int32_t)v3.y };
	V2s s = { .x = (int32_t) p.x, .y = (int32_t) p.y };

	int32_t as_x = s.x - a.x;
	int32_t as_y = s.y - a.y;

	bool s_ab = (b.x - a.x) * as_y - (b.y - a.y) * as_x > 0;

	if (( (c.x - a.x) * as_y - (c.y - a.y) * as_x > 0 ) == s_ab) return false;
	if (( (c.x - b.x) * (s.y - b.y) - (c.y - b.y) * (s.x - b.x)  > 0 ) != s_ab) return false;
	return true;
}

void triangle_draw(Triangle t, uint32_t* buffer, Camera camera, Color color) {

	if (state.wireframe) {
		line_draw(t.v1, t.v2, buffer, color, camera);
		line_draw(t.v1, t.v3, buffer, color, camera);
		line_draw(t.v2, t.v3, buffer, color, camera);
	} else {
		/*
		V2s v1 = get_image_crd(t.v1, camera);
		V2s v2 = get_image_crd(t.v2, camera);
		V2s v3 = get_image_crd(t.v3, camera);

		uint32_t x_min = v1.x;
		if (v2.x < x_min) x_min = v2.x;
		if (v3.x < x_min) x_min = v3.x;
		uint32_t x_max = t.v1.x;
		if (v2.x > x_max) x_max = v2.x;
		if (v3.x > x_max) x_max = v3.x;
		uint32_t y_min = t.v1.y;
		if (v2.y < y_min) y_min = v2.y;
		if (v3.y < y_min) y_min = v3.y;
		uint32_t y_max = t.v1.y;
		if (v2.y > y_max) y_max = v2.y;
		if (v3.y > y_max) y_max = v3.y;

		for(uint32_t x = x_min; x <= x_max; x++) {
		for(uint32_t y = y_min; y <= y_max; y++) {
			V2u p = (V2u) { x, y };
			if (is_point_in_triangle(p, t)) {
				pixel_set(x, y, buffer, color);
			}
		}}
	*/
	}
}

void time_measure_start(struct timespec* t0) {

	*t0 = (struct timespec){0};
	timespec_get(t0, TIME_UTC);
}

double time_measure_end_ms(struct timespec* t1, struct timespec* t0) {

	*t1 = (struct timespec){0};
	timespec_get(t1, TIME_UTC);

	long dns = t1->tv_nsec - t0->tv_nsec;
	double frame_time_ms = (double) dns / (1000 * 1000);

	return frame_time_ms;
}

void event_loop(SDLContext* ctx, uint32_t* buffer, Camera camera) {

	// for fps calculation
	struct timespec t0 = {0};
	struct timespec t1 = {0};
	size_t step = 0;

	bool running = true;

	Triangle tri1 = {
		.v1 = {{ -1.0f, 0.0f, -1.0f }},
		.v2 = {{  1.0f, 0.0f,  1.0f }},
		.v3 = {{  1.0f, 0.0f,  2.0f }}
	};

	while (running) {

		time_measure_start(&t0);
		while (SDL_PollEvent(&ctx->event) != 0) {

			switch (ctx->event.type) {

			case SDL_KEYDOWN:
				float mv_fac = 0.05f;
				if (ctx->event.key.keysym.sym == SDLK_ESCAPE) running = false;
				if (ctx->event.key.keysym.sym == SDLK_g) state.grid_on = !state.grid_on;
				if (ctx->event.key.keysym.sym == SDLK_w) state.wireframe = !state.wireframe;
				if (ctx->event.key.keysym.sym == SDLK_u) {
					V3f dir = (V3f) {{camera.forward.x, 0.0f, camera.forward.z}};
					dir = norm_3f(dir);
					camera.position = add_3f(camera.position, scal_3f(mv_fac, dir));
				}
				if (ctx->event.key.keysym.sym == SDLK_i) {
					V3f dir = (V3f) {{camera.forward.x, 0.0f, camera.forward.z}};
					dir = norm_3f(dir);
					camera.position = sub_3f(camera.position, scal_3f(mv_fac, dir));
				}
				if (ctx->event.key.keysym.sym == SDLK_t) {
					V3f dir = norm_3f(cross_3f(camera.up, camera.forward));
					camera.position = add_3f(camera.position, scal_3f(mv_fac, dir));
				}
				if (ctx->event.key.keysym.sym == SDLK_e) {
					V3f dir = norm_3f(cross_3f(camera.forward, camera.up));
					camera.position = add_3f(camera.position, scal_3f(mv_fac, dir));
				}

				break;

			case SDL_MOUSEMOTION:
				V2f rel = {
					.x = ctx->event.motion.xrel,
					.y = ctx->event.motion.yrel,
				};
				camera_update_mouse(&camera, rel);

			default:
				break;
			}
		}
		if (state.grid_on) grid_draw(buffer, camera);
		/*
		for (size_t i = 0; i < asset_cube.f_count; i++) {
			Triangle t = {
				.v1 = asset_cube.v[asset_cube.f[i].x-1],
				.v2 = asset_cube.v[asset_cube.f[i].y-1],
				.v3 = asset_cube.v[asset_cube.f[i].z-1]
			};

			triangle_draw(t, buffer, camera, GREEN);
		}
		*/

		for (size_t i = 0; i < asset_teapot.f_count; i++) {
			Triangle t = {
				.v1 = asset_teapot.v[asset_teapot.f[i].x-1],
				.v2 = asset_teapot.v[asset_teapot.f[i].y-1],
				.v3 = asset_teapot.v[asset_teapot.f[i].z-1]
			};

			triangle_draw(t, buffer, camera, GREEN);
		}

		//triangle_draw(tri1, buffer, camera, GREEN);
		//V3f origin = {{8.0f, 0.0f, 8.0f}};
		//cube_draw(origin, 2.0f, buffer, RED, camera);

		SDL_UpdateWindowSurface(ctx->window);
		buffer_flush(buffer, ctx->bytes_per_pixel);

		// end time measuring
		double t_ms = time_measure_end_ms(&t1, &t0);
		text_render(string_format("frame time = %.2f ms, FPS = %.2f,"
					"lines drawn = %zu, triangles drawn = %zu\n",
					t_ms, 1/(t_ms/1000), lines_count_global,
					triangle_count_global), 0, 0, buffer, GREEN, 2);
		lines_count_global     = 0;
		triangle_count_global = 0;
	}
}

int main(void) {

	SDLContext* ctx = calloc((size_t) 1, (size_t) sizeof *ctx);

	context_sdl_init(ctx);

	uint32_t* buffer = ctx->surface->pixels;

	Camera camera;
	camera_default_set(&camera);

	event_loop(ctx, buffer, camera);

	memory_free(ctx);
	SDL_Quit();

	return 0;
}

