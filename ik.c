#include "ik.h"

float degrees(float rad) {
    return rad * (180.0 / M_PI);
}

float radians(float deg) {
    return deg * (M_PI / 180.0);
}

float normalize_angle(float angle) {
    angle = fmodf(angle, 360.0);  // Ensure angle is within the range of -360.0 to 360.0
    
    // Convert negative angles to their corresponding positive angles within the same position
    if (angle < 0) angle += 360.0;

    // Ensure angle is within the range of 0.0 to 180.0
    if (angle > 180.0) angle = 360.0 - angle;

    return angle;
}

float *get_target(SpiderLeg *leg) {
    return leg->joints[3];
}


void set_angles(SpiderLeg *leg, float angles[3]) {
    leg->theta1 = normalize_angle(angles[0]);
    leg->theta2 = normalize_angle(angles[1]);
    leg->theta3 = normalize_angle(angles[2]);

    set_pwm_angle(SERVO_CHANNEL_10, (int)leg->theta1, PWM_FREQ);
    set_pwm_angle(SERVO_CHANNEL_11, (int)leg->theta2,PWM_FREQ );
    set_pwm_angle(SERVO_CHANNEL_12, (int)leg->theta3, PWM_FREQ);

    printf("Theta1: %.4f degrees\n", leg->theta1);
    printf("Theta2: %.4f degrees\n", leg->theta2);
    printf("Theta3: %.4f degrees\n", leg->theta3);
}

void move_to_angle(SpiderLeg *leg, float target_angles[3], float velocity) {
    float increment = velocity / 100.0;
    while (fabs(leg->theta1 - target_angles[0]) > 0.01 || 
           fabs(leg->theta2 - target_angles[1]) > 0.01 || 
           fabs(leg->theta3 - target_angles[2]) > 0.01) { // Adjust the threshold as needed
        float angle1 = (leg->theta1 + increment);
        float angle2 = (leg->theta2 + increment);
        float angle3 = (leg->theta3 + increment);
        if(angle1 >= target_angles[0]){
            angle1 = target_angles[0];
        }
        if(angle2 >= target_angles[1]){
            angle2 = target_angles[1];
        }
        if(angle3 >= target_angles[2]){
            angle3 = target_angles[2];
        }

        if(angle1 == target_angles[0] && angle2 == target_angles[1] && angle3 == target_angles[2]){
            printf("target achieved\n");
        }
        
        set_pwm_angle(SERVO_CHANNEL_10, angle1, PWM_FREQ);
        set_pwm_angle(SERVO_CHANNEL_11, angle2, PWM_FREQ);
        set_pwm_angle(SERVO_CHANNEL_12, angle3, PWM_FREQ); // Fixed typo here

        leg->theta1 = angle1;
        leg->theta2 = angle2;
        leg->theta3 = angle3;
        printf("Theta1: %.4f degrees\n", leg->theta1);
        printf("Theta2: %.4f degrees\n", leg->theta2);
        printf("Theta3: %.4f degrees\n", leg->theta3);
        usleep(10000);
    }
}


void init_DH_params(DHParameters *params, float alpha, float a, float d, float theta)
{
    params->alpha = alpha;
    params->a = a;
    params->d = d;
    params->theta = theta;
}

void create_DH_matrix(const DHParameters *params, gsl_matrix *matrix)
{
    float alpha = params->alpha;
    float theta = params->theta;

    // Fill the DH matrix
    gsl_matrix_set(matrix, 0, 0, cos(theta));
    gsl_matrix_set(matrix, 0, 1, -sin(theta) * cos(alpha));
    gsl_matrix_set(matrix, 0, 2, sin(theta) * sin(alpha));
    gsl_matrix_set(matrix, 0, 3, params->a * cos(theta));

    gsl_matrix_set(matrix, 1, 0, sin(theta));
    gsl_matrix_set(matrix, 1, 1, cos(theta) * cos(alpha));
    gsl_matrix_set(matrix, 1, 2, -cos(theta) * sin(alpha));
    gsl_matrix_set(matrix, 1, 3, params->a * sin(theta));

    gsl_matrix_set(matrix, 2, 0, 0.0);
    gsl_matrix_set(matrix, 2, 1, sin(alpha));
    gsl_matrix_set(matrix, 2, 2, cos(alpha));
    gsl_matrix_set(matrix, 2, 3, params->d);

    gsl_matrix_set(matrix, 3, 0, 0.0);
    gsl_matrix_set(matrix, 3, 1, 0.0);
    gsl_matrix_set(matrix, 3, 2, 0.0);
    gsl_matrix_set(matrix, 3, 3, 1.0);
}


void print_DH_matrix(const DHMatrix *matrix) {
    printf("DH Matrix:\n");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%f\t", matrix->matrix[i][j]);
        }
        printf("\n");
    }
}

void multiply_DH_matrices(const DHMatrix *matrix1, const DHMatrix *matrix2, DHMatrix *result) {
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            result->matrix[i][j] = 0;
            for (k = 0; k < 4; k++) {
                result->matrix[i][j] += matrix1->matrix[i][k] * matrix2->matrix[k][j];
            }
        }
    }
}

void calculate_DH_transformation(const DHParameters *params_array, int num_links, gsl_matrix *result, gsl_matrix *intermediate_matrices[])
{
    // Identity matrix
    gsl_matrix *identityMatrix = gsl_matrix_alloc(4, 4);
    gsl_matrix_set_identity(identityMatrix);

    // Iterate through each link and calculate intermediate matrices
    for (int i = 0; i < num_links; i++) {
        gsl_matrix *linkMatrix = gsl_matrix_alloc(4, 4);
        create_DH_matrix(&params_array[i], linkMatrix);
        
        // Multiply identityMatrix and linkMatrix
        gsl_blas_dgemm(CblasNoTrans, CblasNoTrans, 1.0, identityMatrix, linkMatrix, 0.0, result);
        
        // Store the result in the intermediate matrices array
        gsl_matrix_memcpy(intermediate_matrices[i], result);

        // Update the identityMatrix with the multiplied matrix
        gsl_matrix_memcpy(identityMatrix, result);
        
        // Free the linkMatrix since it's not needed anymore
        gsl_matrix_free(linkMatrix);
    }

    // Free the identityMatrix
    gsl_matrix_free(identityMatrix);
}


void forward_kinematics(SpiderLeg *leg, float angles[3], gsl_matrix *intermediate_matrices[])
{
    // Convert to radians
    float theta1 = radians(angles[0]);
    float theta2 = radians(angles[1]) - radians(90); // -90 because of angle offset of mounting servo
    float theta3 = -radians(angles[2]) + radians(90); // -90 because of angle offset of mounting_servo

    DHParameters params_array[NUM_LINKS];
    init_DH_params(&params_array[0], radians(90.0), COXA_LENGTH, 0.0, (theta1 + radians(90.0)));
    init_DH_params(&params_array[1], radians(0.0), FEMUR_LENGTH, 0.0, theta2);
    init_DH_params(&params_array[2], radians(-90.0), TIBIA_LENGTH, 0.0, (theta3 - radians(90.0)));
    init_DH_params(&params_array[3], radians(90.0), 0.0, 0.0, radians(-90.0));

    gsl_matrix *trans_matrix = gsl_matrix_alloc(4, 4);
    calculate_DH_transformation(params_array, NUM_LINKS, trans_matrix, intermediate_matrices);

    float x = fabs(gsl_matrix_get(trans_matrix, 0, 3));
    if (x < 0) {
        x = 0;
    }
    float y = gsl_matrix_get(trans_matrix, 1, 3);
    float z = gsl_matrix_get(trans_matrix, 2, 3);

    float position[3] = {x, y, z};

    // Update leg joints end-effector
    for (int i = 0; i < 3; i++) {
        leg->joints[3][i] = position[i];
    }

        printf("end-effector position: x = %.2f, y = %.2f, z = %.2f\n", leg->joints[3][0], leg->joints[3][1], leg->joints[3][2]);

    gsl_matrix_free(trans_matrix);
}

void inverse_kinematics(SpiderLeg *leg, float target_positions[3], gsl_matrix *intermediate_metrices[])
{
    // float x;
    // if((target_positions[0]) < 0){
    //    if (target_positions[0] + leg->joints[3][0] < 0){
    //         x = leg->joints[3][0];
    //         printf("not posible\n");
    //     }else{
    //         x = target_positions[0] + leg->joints[3][0];
    //     }

    // } else if(target_positions[0] > 0){
    //     x = target_positions[0] + leg->joints[3][0];
    // } else {
    //     x = leg->joints[3][0];
    // }

    // printf("x = %.2f\n", x);

    // float y;
    // if (target_positions[1] < 0){
    //     if (target_positions[1] + leg->joints[3][1] < 0){
    //         y = target_positions[1] + leg->joints[3][1];
    //         y = fabs(y);
    //     } else {
    //         y = target_positions[1] + leg->joints[3][1];
    //     }
    // } else if (target_positions[1] > 0){
    //     y = target_positions[1] + leg->joints[3][1];
    // } else {
    //     y = leg->joints[3][1];
    // }

    // printf("y = %.2f\n", y);

    // float z;
    // if (target_positions[2] < 0){
    //     z = target_positions[2] + leg->joints[3][2];
    // } else if(target_positions[2] > 0 ){
    //     z = target_positions[2] - leg->joints[3][2];
    // } else {
    //     z = leg->joints[3][2];
    // }
    // printf("z = %.2f\n", z);
    // z = fabs(z);
    float x = target_positions[0];
    float y = target_positions[1];
    float z = target_positions[2];
    //angle antara coxa dengan horizontal plane
    float theta1 = atan2(x, y);

    float P = sqrt(pow(x, 2) + powf(y, 2)) - COXA_LENGTH;

    float G = sqrt(pow(z, 2) + pow(P, 2));

    float alpha = atan2(z, P);

    float gamma_cos = (pow(FEMUR_LENGTH, 2) + pow(G, 2) - pow(TIBIA_LENGTH, 2)) / (2 * FEMUR_LENGTH * G);
    float gamma = acos(gamma_cos);

    float beta_cos = (pow(FEMUR_LENGTH, 2) + pow(TIBIA_LENGTH, 2) - pow(G, 2)) / (2 * FEMUR_LENGTH * TIBIA_LENGTH);
    float beta = acos(beta_cos);

    float theta2 = M_PI/2 + (gamma - fabs(alpha));

    float theta3 = M_PI - beta;

    float angles[3] = {degrees(theta1), degrees(theta2), degrees(theta3)};
    move_to_angle(leg, angles, 100);
    forward_kinematics(leg, angles, intermediate_metrices);
    printf("theta1 = %.2f, theta2 = %.2f, theta3 = %.2f\n", degrees(theta1), degrees(theta2), degrees(theta3));
}

void adjust_coordinate(float x, float y, float z, LegPosition position, float *adj_x, float *adj_y, float *adj_z)
{
    switch (position){
        case KANAN_DEPAN:
            *adj_x = x;
            *adj_y = y;
            *adj_z = z;
            break;
        case KANAN_BELAKANG:
            *adj_x = -y;
            *adj_y = x;
            *adj_z = z;
            break;
        case KIRI_BELAKANG:
            *adj_x = -x;
            *adj_y = -y;
            *adj_z = z;
            break;
        case KIRI_DEPAN:
            *adj_x = y;
            *adj_y = -x;
            *adj_z = z;
            break;
    }
}