#include "ik.h"
#include "move.h"
#include "pwm_servo.h"
#include "interrupt.h"



int main(void)
{
    // Initialize PCA9685 if necessary
    PCA9685_init();

    initialize_all_legs();

    // Set initial angles using forward kinematics
    stand_position();

    init_interrupt();

    while (1) {
        // Check if the switch is turned on
        if (is_program_running) {
            // If the switch is on, move forward
            move_forward();     
        } else {
            stand_position();
            // If the switch is off, pause the program
            // You can add additional functionality here if needed
            // For example, you can keep the robot in its current position
            // Or you can add logic to respond to other inputs or events
        }
    }


    return 0;
}
