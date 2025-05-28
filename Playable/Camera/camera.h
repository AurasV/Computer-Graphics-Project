#ifndef CAMERA_H
#define CAMERA_H

// Include GLM for vector and matrix operations
// Make sure these paths are correct relative to your project setup
#include "../dependente/glm/glm.hpp"
#include "../dependente/glm/gtc/matrix_transform.hpp"
#include "../dependente/glm/gtc/type_ptr.hpp" // Included for potential glm::value_ptr usage if needed

class Camera
{
public:
    // Camera properties:
    // position: The current location of the camera in world space.
    // viewDirection: A normalized vector indicating where the camera is looking.
    // up: A normalized vector indicating the camera's "up" direction (usually (0, 1, 0) for a standard camera).
    glm::vec3 position;
    glm::vec3 viewDirection;
    glm::vec3 up;

    // Constructor: Initializes the camera with its position, view direction, and up vector.
    // The implementation for this constructor will be in camera.cpp.
    Camera(glm::vec3 position, glm::vec3 viewDirection, glm::vec3 up);

    // Movement functions:
    // These functions move the camera based on an input speed.
    void keyboardMoveFront(float speed);
    void keyboardMoveBack(float speed);
    void keyboardMoveLeft(float speed);
    void keyboardMoveRight(float speed);
    void keyboardMoveUp(float speed);
    void keyboardMoveDown(float speed);

    // Rotation functions:
    // These functions rotate the camera around its local X (pitch) and Y (yaw) axes.
    void rotateOx(float angle); // Rotate around local X-axis (pitch)
    void rotateOy(float angle); // Rotate around local Y-axis (yaw)
};

#endif // CAMERA_H