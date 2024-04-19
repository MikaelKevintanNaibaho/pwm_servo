#ifndef MOVE_H
#define MOVE_H

#include "ik.h"
#include "gsl/gsl_spline.h"
#include <stdio.h>

typedef enum {
    MOVE_FORWARD,
    MOVE_BACKWARD,
    MOVE_LEFT,
    MOVE_RIGHT,
    TURN_LEFT,
    TURN_RIGHT
} MovementCommand;

struct bezier2d {
    float *xpos;
    float *ypos;
    int npoints;
};

#define NUM_POINTS 50

void bezier2d_init(struct bezier2d *curve);
void bezier2d_addPoint(struct bezier2d *curve, float x, float y);
void bezier2d_getPos(struct bezier2d *curve, float t, float *xret, float *yret);
void bezier2d_generate_curve(struct bezier2d *curve, float startx, float startz, float controlx, float controlz, float endx, float endz);

void generate_walk_trajectory(struct bezier2d *curve, SpiderLeg *leg, float stride_length, float swing_height);
void print_trajectory(struct bezier2d *curve, int num_points) ;
void save_trajectory_points(struct bezier2d *curve, const char *filename, int num_points) ;
void update_leg_position_with_velocity(struct bezier2d *curve, int number_points, SpiderLeg *leg, gsl_matrix *intermediate_matrices[]);

#endif //MOVE_H