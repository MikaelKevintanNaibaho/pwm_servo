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


void generate_walk_trajectory(struct bezier2d *curve, SpiderLeg *leg, float stride_length, float swing_height, LegPosition position_leg) 
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
    printf("startx : %f", startx);

    float endx = startx - stride_length / 2;
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
void update_leg_position_with_velocity(struct bezier2d *curve, int number_points, SpiderLeg *leg, LegPosition position_leg)
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
        inverse_kinematics(leg, target_positions, position_leg);

        // Introduce delay based on desired velocity
        long delay = (long)(dt * 1e6);
        usleep(delay);
    }
}


void walk_forward(SpiderLeg *legs[NUM_LEGS], float stride_length, float swing_height, int num_points, LegPosition position_leg[NUM_LEGS])
{
    struct bezier2d curve[NUM_LEGS];
    for (int i = 0; i < NUM_LEGS; i++) {
        bezier2d_init(&curve[i]);
    }

    for (int i = 0; i < NUM_LEGS; i++) {
        generate_walk_trajectory(&curve[i], legs[i], stride_length, swing_height, position_leg[i]);
    }

    //update leg positions for each leg
    for (int i = 0; i < NUM_LEGS; i++) {
        update_leg_position_with_velocity(&curve[i], num_points, legs[i], position_leg[i]);
        float x = legs[i]->joints[3][0];
        float y = legs[i]->joints[3][1];
        float z = legs[i]->joints[3][2];
        float target[3] = {x - 50.0, y, z}; 
        inverse_kinematics(legs[i], target, position_leg[i]);
    }
    
    usleep(100000);

    // usleep(500000);
}

void crawl_gait(SpiderLeg *legs[NUM_LEGS], LegPosition position_leg[NUM_LEGS])
{
    struct bezier2d curves[NUM_LEGS];
    struct bezier2d stright_back[NUM_LEGS];

    for (int i = 0; i < NUM_LEGS; i++){
        bezier2d_init(&curves[i]);
        bezier2d_init(&stright_back[i]);
    }

    for (int i = 0; i < NUM_LEGS; i++) {
        generate_walk_trajectory(&curves[i], legs[i], STRIDE_LENGTH, SWING_HEIGTH, position_leg[i]);
        generate_stright_back_trajectory(&stright_back[i], legs[i], STRIDE_LENGTH);
    }

    while (1) {
    // Update leg positions for leg1
    update_leg_position_with_velocity(&curves[0], NUM_POINTS, legs[0], position_leg[0]);
    usleep(10000); // Introduce delay between leg movements
    update_leg_position_with_velocity(&stright_back[2], NUM_POINTS, legs[2], position_leg[2]);
    update_leg_position_with_velocity(&curves[2], NUM_POINTS, legs[2], position_leg[2]);
    usleep(10000); // Introduce delay between leg movements
    update_leg_position_with_velocity(&stright_back[0], NUM_POINTS, legs[0], position_leg[0] );

    // Update leg positions for leg2
    update_leg_position_with_velocity(&curves[1], NUM_POINTS, legs[1], position_leg[1]);
    usleep(10000); // Introduce delay between leg movements
    update_leg_position_with_velocity(&stright_back[3], NUM_POINTS, legs[3], position_leg[3] );

    // Update leg positions for leg4
    update_leg_position_with_velocity(&curves[3], NUM_POINTS, legs[3], position_leg[3]);
    usleep(10000); // Introduce delay between leg movements
    update_leg_position_with_velocity(&stright_back[1], NUM_POINTS, legs[1], position_leg[1] );
}


    // Free memory allocated for curves and straight backs
    for (int i = 0; i < NUM_LEGS; i++) {
        free(curves[i].xpos);
        free(curves[i].ypos);
        free(stright_back[i].xpos);
        free(stright_back[i].ypos);
    }
}
float calculate_phase_shift(int leg_index) {
  return (2 * M_PI * leg_index) / NUM_LEGS;
}

void generate_gait_trajectories(struct bezier2d *trajectories, SpiderLeg *legs) {
  for (int leg_index = 0; leg_index < NUM_LEGS; leg_index++) {
    float phase_shift = calculate_phase_shift(leg_index);

    // Get leg starting position from leg data
    float startx = legs[leg_index].joints[3][0];
    float startz = legs[leg_index].joints[3][2];

    // Adjust control point based on phase shift for wave gait pattern
    float controlx_offset = STRIDE_LENGTH * sin(phase_shift) / 2;
    float controlz_offset = SWING_HEIGTH * cos(phase_shift);
    float controlx = startx + controlx_offset;
    float controlz = startz + controlz_offset;

    // Endpoint for forward motion (modify for different leg patterns)
    float endx_forward = startx + STRIDE_LENGTH;
    float endz_forward = startz;

    // Generate trajectory for current leg with phase shift considered
    bezier2d_generate_curve(&trajectories[leg_index], startx, startz, controlx, controlz, endx_forward, endz_forward);
  }
}


void wave_gait(SpiderLeg *legs[NUM_LEGS], float stride_length, float swing_height, LegPosition leg_position[NUM_LEGS]) {   struct bezier2d curve[NUM_LEGS];
    struct bezier2d back[NUM_LEGS];
    for (int i = 0; i < NUM_LEGS; i++) {
        bezier2d_init(&curve[i]);
        bezier2d_init(&back[i]);
    }

    // Generate trajectory
    for (int i = 0; i < NUM_LEGS; i++) {
            generate_walk_trajectory(&curve[i], legs[i], stride_length, swing_height, leg_position[i]);
            generate_stright_back_trajectory(&back[i], legs[i], stride_length);
    }

    for (int i = 0; i < NUM_POINTS; i++){
        for (int j = 0; j < NUM_LEGS; j++){
            update_leg_position_with_velocity(&curve[j], NUM_POINTS, legs[j], leg_position[j]);
            update_leg_position_with_velocity(&back[j], 200, legs[j], leg_position[j]);
        }
    }


}
