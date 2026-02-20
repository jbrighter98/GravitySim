#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec2 position;
    float zoom;

    Camera() : position(0.0f, 0.0f), zoom(1.0f) {}

    glm::mat4 getViewMatrix() {
        // We move the world in the opposite direction of the camera
        return glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f));
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) {
        // Applying zoom: smaller range = zoomed in
        float left = -aspectRatio * zoom;
        float right = aspectRatio * zoom;
        float bottom = -1.0f * zoom;
        float top = 1.0f * zoom;
        return glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    }
};
#endif