#ifndef _LALG_H
#define _LALG_H
#include <stdint.h>

typedef struct {
	uint32_t x;
	uint32_t y;
} V2u;

typedef struct {
	int32_t x;
	int32_t y;
} V2s;

typedef union {
	struct {
		float x;
		float y;
	};
	float arr[2];
} V2f;

typedef union {
	struct {
		float x;
		float y;
		float z;
	};
	float arr[3];
} V3f;

typedef union {
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t z;
	};
	uint32_t arr[3];
} V3u;

typedef struct {
	V3f v1;
	V3f v2;
	V3f v3;
} Triangle;

typedef struct {
	V2u v1;
	V2u v2;
	V2u v3;
} Triangle_2u;

typedef union {
	struct {
		float m00, m01, m02;
		float m10, m11, m12;
		float m20, m21, m22;
	};
	float arr[9];
} M3f;

static inline V2u v2s_to_v2u(V2s a) {

	V2u res = {0};

	res.x = (uint32_t)a.x;
	res.y = (uint32_t)a.y;

	return res;
}

static inline M3f id_3f() {

	M3f res = {0};

	res = (M3f) {{
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f
	}};

	return res;
}

static inline float dot_3f(V3f a, V3f b) {

	float res = 0;

	res = a.x*b.x+a.y*b.y+a.z*b.z;

	return res;
}

static inline M3f rot_xy_3f(float angle) {

	M3f res = {0};

	res = (M3f) {{
		cosf(angle), -sinf(angle), 0.0f,
		sinf(angle),  cosf(angle), 0.0f,
		0.0f,                0.0f, 1.0f
	}};

	return res;
}

static inline M3f rot_xz_3f(float angle) {

	M3f res = {0};


	res = (M3f) {{
		 cosf(angle), 0.0f, sinf(angle),
		        0.0f, 1.0f,        0.0f,
		-sinf(angle), 0.0f, cosf(angle),
	}};

	return res;
}

static inline M3f rot_yz_3f(float angle) {

	M3f res = {0};


	res = (M3f) {{
		1.0f,        0.0f, 	   0.0f,
		0.0f, cosf(angle), -sinf(angle),
		0.0f, sinf(angle),  cosf(angle),
	}};

	return res;
}

static inline V3f mul_m3f_v3f(M3f m, V3f a) {

	V3f res = {0};

	for (size_t i = 0; i < 3; i++) {
	for (size_t j = 0; j < 3; j++) {
		res.arr[i] += m.arr[i + j*3] * a.arr[j];
	}}

	return res;
}

static inline V3f cross_3f(V3f a, V3f b) {

	V3f res = {
		.x = a.y*b.z - a.z*b.y,
		.y = a.z*b.x - a.x*b.z,
		.z = a.x*b.y - a.y*b.x
	};

	return res;
}

static inline V3f norm_3f(V3f a) {

	V3f res = {0};

	float len = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
	if (len > 1e-8) {
		res.x = a.x / len;
		res.y = a.y / len;
		res.z = a.z / len;
	}

	return res;
}

static inline float length_3f(V3f a) {

	float res = 0.0f;

	res = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);

	return res;
}

static inline V3f sub_3f(V3f a, V3f b) {

	V3f res = {0};

	res.x = a.x - b.x;
	res.y = a.y - b.y;
	res.z = a.z - b.z;

	return res;
}

static inline V3f add_3f(V3f a, V3f b) {

	V3f res = {0};

	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;

	return res;
}

static inline V3f scal_3f(float fac, V3f a) {

	V3f res = {0};

	res.x = fac * a.x;
	res.y = fac * a.y;
	res.z = fac * a.z;

	return res;
}

static inline V3f intersect_z(V3f a, V3f b, float z) {

	V3f res = {0};

	float dz = b.z - a.z;
	if (dz*dz < 1e-8f) return a;

	float lambda = (z - a.z) / dz;

	res.x = a.x + lambda * (b.x - a.x);
	res.y = a.y + lambda * (b.y - a.y);
	res.z = z;

	return res;
}

static inline V2s intersect_x(V2s a, V2s b, int32_t x) {

	V2s res = {0};

	float dx = (float)b.x - (float)a.x;
	if (dx*dx < 1e-8f) return a;

	float lambda = ((float)x - (float)a.x) / dx;

	res.x = x;
	res.y = a.y + (int32_t)(lambda * ((float)b.y - (float)a.y));

	return res;
}

static inline V2s intersect_y(V2s a, V2s b, int32_t y) {

	V2s res = {0};

	float dy = (float)b.y - (float)a.y;
	if (dy*dy < 1e-8f) return a;

	float lambda = ((float)y - (float)a.y) / dy;

	res.x = a.x + (int32_t)(lambda * ((float)b.x - (float)a.x));
	res.y = y;

	return res;
}

static inline V3f rot_rod_3f(V3f vector, V3f axis, float angle) {

	V3f res = {0};

	float cosine = cosf(angle);
	float sine   = sinf(angle);

	res = add_3f(add_3f(scal_3f(cosine, vector), scal_3f(sine, cross_3f(axis, vector))), scal_3f((1.0f-cosine)*dot_3f(axis, vector), axis));

	return res;
}
#endif

