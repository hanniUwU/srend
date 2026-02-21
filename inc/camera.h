#ifndef CAMERA_H
#define CAMERA_H

#include "./lalg.h"

typedef struct {
        V3f position;
	V3f forward;
        V3f up;
	float yaw;
        float fovy;
	float znear;
	float zfar;
} Camera;


void camera_default_set(Camera* c);
void camera_update_mouse(Camera* camera, V2f rel);
void camera_info_print(Camera camera);

#endif

