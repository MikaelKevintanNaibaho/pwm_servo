#include "move.h"
#include <stdio.h>
#include <time.h>

void bezier2d_init(struct bezier2d *curve) {
    curve->xpos = NULL;
    curve->ypos = NULL;
    curve->npoints = 0;
}

void bezier2d_addPoint(struct bezier2d *curve, float x, float y) {
    curve->npoints++;
    curve->xpos = (float *)realloc(curve->xpos, curve->npoints * sizeof(float));
    curve->ypos = (float *)realloc(curve->ypos, curve->npoints * sizeof(float));
    curve->xpos[curve->npoints - 1] = x;
    curve->ypos[curve->npoints - 1] = y;
}

void bezier2d_getPos(struct bezier2d *curve, float t, float *xret, float *yret) {
    int ii, ij;
    float *x, *y;

    if (curve->npoints == 0) {
        *xret = 0;
        *yret = 0;
        return;
    }

    x = (float *)malloc(curve->npoints * sizeof(float));
    y = (float *)malloc(curve->npoints * sizeof(float));

    // load with current points
    for (ii = 0; ii < curve->npoints; ii++) {
        x[ii] = curve->xpos[ii];
        y[ii] = curve->ypos[ii];
    }

    // iterate over levels
    for (ii = 0; ii < curve->npoints - 1; ii++) {
        for (ij = 0; ij < curve->npoints - ii - 1; ij++) {
            x[ij] = (1.0 - t) * x[ij] + t * x[ij + 1];
            y[ij] = (1.0 - t) * y[ij] + t * y[ij + 1];
        }
    }

    *xret = x[0];
    *yret = y[0];

    free(x);
    free(y);
}

void bezier2d_generate_curve(struct bezier2d *curve, float startx, float startz, float controlx, float controlz, float endx, float endz){
    bezier2d_addPoint(curve, startx, startz);
    bezier2d_addPoint(curve, controlx, controlz);
    bezier2d_addPoint(curve, endx, endz);
}


void generate_walk_trajectory(struct bezier2d *curve, SpiderLeg *leg, float stride_length, float swing_height) 
{
    //get current position
    float startx = leg->joints[3][0];
    float startz = leg->joints[3][2];

    //control points
    float controlx = (startx + stride_length) / 2;
    printf("controlx = %f \t", controlx);
    float controlz = startz + 2 * swing_height;
    printf("controlz = %f\n", controlz);
    float endx_forward = startx + stride_length;
    float endz_forward = startz;
    //buar bezier curve
    bezier2d_generate_curve(curve, startx, startz, controlx, controlz, endx_forward, endz_forward);

    // // Append straight line for moving backward
    // bezier2d_addPoint(curve, endx_forward, endz_forward);
    // bezier2d_addPoint(curve, startx, startz);

}

void bezier2d_generate_straight_back(struct bezier2d *stright_back, float startx, float startz, float endx, float endy)
{
    bezier2d_addPoint(stright_back, startx, startz);
    bezier2d_addPoint(stright_back, endx, endy);
}

void generate_stright_back_trajectory(struct bezier2d *stright_back, SpiderLeg *leg, float stride_length)
{
    float startx = leg->joints[3][0];
    float startz = leg->joints[3][2];

    float endx = startx - stride_length;
    float endz = startz;

    bezier2d_generate_straight_back(stright_back, startx, startz, endx, endz);
}


void print_trajectory(struct bezier2d *curve, int num_points) {
    printf("Trajectory Points:\n");
    for (int i = 0; i <= num_points; i++) {
        float t = (float)i / num_points;
        float x, y;
        bezier2d_getPos(curve, t, &x, &y);
        printf("Point %d: (%.2f, %.2f)\n", i, x, y);
    }
}


void save_trajectory_points(struct bezier2d *curve, const char *filename, int num_points) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    for (int i = 0; i <= num_points; i++) {
        float t = (float)i / num_points;
        float x, z;
        bezier2d_getPos(curve, t, &x, &z);
        fprintf(file, "%.2f %.2f\n", x, z);
    }

    fclose(file);
}
void update_leg_position_with_velocity(struct bezier2d *curve, int number_points, SpiderLeg *leg, gsl_matrix *intermediate_matrices[])
{
    printf("updating leg position with fixed delay\n");

    float total_distance = 0.0;
    float prev_x, prev_z;
    bezier2d_getPos(curve, 0, &prev_x, &prev_z); //get poin pertama

    for (int i = 0; i <= number_points; i++) {
        float t = (float)i / number_points;
        float x,z;
        bezier2d_getPos(curve, t, &x, &z); 

        //hitung jarak antara dua titik
        float dx = x - prev_x;
        float dz = z - prev_z;
        total_distance += sqrt(dx * dx + dz * dz);

        prev_x = x;
        prev_z = z;
    }

    //hitung velocity base on total distance dan disired duration
    float desired_duration = DESIRED_TIME;
    float average_velocity = total_distance / desired_duration;

    //hitung interval antara dua titik
    float dt = desired_duration / number_points;

    //lakukan untuk setiap titik dan control velocity
    // Iterate over points and control velocity
    for (int i = 0; i <= number_points; i++) {
        float t = (float)i / number_points;
        float x, z;
        bezier2d_getPos(curve, t, &x, &z);

        // Print position of current point
        printf("Point %d: (%.2f, %.2f)\n", i, x, z);

        // Calculate target position for inverse kinematics
        float y = leg->joints[3][1];
        float target_positions[3] = {x, y, z};

        // Update leg position using inverse kinematics
        inverse_kinematics(leg, target_positions, intermediate_matrices);

        // Introduce delay based on desired velocity
        long delay = (long)(dt * 1e6);
        usleep(delay);
    }
}


void walk_forward(SpiderLeg *leg, gsl_matrix *intermediate_matrices[], float stride_length, float swing_height, int num_points)
{
    struct bezier2d curve;
    bezier2d_init(&curve);

    generate_walk_trajectory(&curve, leg, stride_length, swing_height);

    update_leg_position_with_velocity(&curve, num_points, leg, intermediate_matrices);
    usleep(100000);
    
    struct bezier2d back;
    bezier2d_init(&back);
    generate_stright_back_trajectory(&back, leg, 100);

    update_leg_position_with_velocity(&back, 10, leg, intermediate_matrices);

    usleep(500000);
}