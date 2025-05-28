#include "camera.h" // Include the corresponding header file

// Constructor implementation
// Initializes the camera's position, view direction, and up vector using an initializer list.
Camera::Camera(glm::vec3 initialPosition, glm::vec3 initialViewDirection, glm::vec3 initialUp)
    : position(initialPosition), viewDirection(initialViewDirection), up(initialUp)
{
    // Any additional initialization logic for the camera can go here.
    // For example, normalizing vectors if they aren't guaranteed to be normalized on input.
    // Or setting up initial camera matrices if you were storing them as members.
    viewDirection = glm::normalize(viewDirection);
    up = glm::normalize(up);
}

// Moves the camera forward along its view direction
void Camera::keyboardMoveFront(float cameraSpeed)
{
    position += viewDirection * cameraSpeed * 50.0f; // Multiplier 50.0f for faster movement
}

// Moves the camera backward along its view direction
void Camera::keyboardMoveBack(float cameraSpeed)
{
    position -= viewDirection * cameraSpeed * 50.0f; // Multiplier 50.0f for faster movement
}

// Moves the camera left (strafing)
// This requires calculating a 'right' vector, which is perpendicular to viewDirection and up.
void Camera::keyboardMoveLeft(float cameraSpeed)
{
    // Calculate the right vector (cross product of viewDirection and up)
    // and normalize it to ensure consistent speed.
    glm::vec3 right = glm::normalize(glm::cross(viewDirection, up));
    position -= right * cameraSpeed * 50.0f; // Move left
}

// Moves the camera right (strafing)
void Camera::keyboardMoveRight(float cameraSpeed)
{
    // Calculate the right vector (cross product of viewDirection and up)
    // and normalize it to ensure consistent speed.
    glm::vec3 right = glm::normalize(glm::cross(viewDirection, up));
    position += right * cameraSpeed * 50.0f; // Move right
}

// Moves the camera upward along its local up vector
void Camera::keyboardMoveUp(float cameraSpeed)
{
    position += up * cameraSpeed;
}

// Moves the camera downward along its local up vector
void Camera::keyboardMoveDown(float cameraSpeed)
{
    position -= up * cameraSpeed;
}

// Rotates the camera around its local X-axis (pitch)
// This will change the viewDirection and potentially the up vector.
void Camera::rotateOx(float angle)
{
    // Calculate the right vector, which is the axis of rotation for pitching
    glm::vec3 right = glm::normalize(glm::cross(viewDirection, up));

    // Create a rotation matrix around the 'right' vector
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), right);

    // Apply the rotation to the viewDirection and up vector
    viewDirection = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(viewDirection, 0.0f)));
    up = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(up, 0.0f)));
}

// Rotates the camera around its local Y-axis (yaw)
// This rotates around the global Y-axis or the camera's current 'up' vector.
void Camera::rotateOy(float angle)
{
    // Create a rotation matrix around the 'up' vector (yaw axis)
    // Using the camera's own up vector for local yaw.
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), up);

    // Apply the rotation to the viewDirection
    viewDirection = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(viewDirection, 0.0f)));
    // Note: The 'up' vector might not need to be updated if you're rotating purely around it.
    // However, if you are also changing pitch, the 'up' vector would have been updated by rotateOx.
}