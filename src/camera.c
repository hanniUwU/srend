#include "../inc/camera.h"

#include "../inc/lalg.h"

#include <math.h>
#include <stdio.h>

void camera_default_set(Camera* c) {

	c->position = (V3f) {{ 4.0f, 1.0f, -12.0f }};
	c->forward  = (V3f) {{ 0.0f, 0.0f,   1.0f }};
	c->up 	    = (V3f) {{ 0.0f, 1.0f,   0.0f }};
	c->fovy     = 80.0f;
	c->znear    = 0.5f;
	c->zfar     = 100.0f;
}

void camera_update_mouse(Camera* camera, V2f rel) {

	// sets mouse speed
	float scale = 1e-3;

	V3f v_y = (V3f) {{ 0.0f, 1.0f, 0.0f}};
	// x-direction
	float angle_x   = -scale * rel.x;
	camera->forward = norm_3f( rot_rod_3f(camera->forward, v_y , angle_x) );

	// y-direction
	float angle_y     = -scale * rel.y;
	V3f right         = norm_3f( cross_3f(camera->forward, v_y) );
	camera->forward   = norm_3f( rot_rod_3f(camera->forward, right, angle_y) );
	camera->forward.y = maxf(camera->forward.y, -0.9);
	camera->up        = norm_3f( cross_3f(right, camera->forward) );
}

void camera_info_print(Camera camera) {

	V3f right = norm_3f(cross_3f(camera.forward, camera.up));
	printf("position = (%.2f, %.2f, %.2f)\n", camera.position.x, camera.position.y, camera.position.z);
	printf("forward  = (%.2f, %.2f, %.2f)\n", camera.forward.x, camera.forward.y, camera.forward.z);
	printf("up       = (%.2f, %.2f, %.2f)\n", camera.up.x, camera.up.y, camera.up.z);
	printf("right    = (%.2f, %.2f, %.2f)\n", right.x, right.y, right.z);
}

