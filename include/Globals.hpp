#ifndef GLOBALS_H
#define GLOBALS_H

#include <vector>

#include "Circle.hpp"
#include "Shader.hpp"


struct CelestialBody {
    char ID[256];
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 acceleration;
    float mass;
    float radius;
    bool exists;
    glm::vec4 color;
    bool isDebris;
    float lifeTime = 1.0f;    // Current life (1.0 = full, 0.0 = gone)
    float decaySpeed = 0.0f;

    CelestialBody(const char* id, glm::vec2 pos, float m, float r, glm::vec4 color, bool isDebris = false) 
        : position(pos), velocity(0.0f, 0.0f), acceleration(0.0f, 0.0f), mass(m), radius(r), exists(true), color(color), isDebris(isDebris) {
            std::strncpy(ID, id, 255);
            ID[255] = '\0';
            if (isDebris) {
                decaySpeed = 0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.3f; // Random decay 2-5 seconds
            }
        }
};


struct AppState {
    std::unique_ptr<Shader> myShader;
    std::unique_ptr<Shader> gridShader;
    std::unique_ptr<Circle> bodyShape;
    GLFWwindow* window;

    std::vector<CelestialBody> bodies; 

    float lastFrame = 0.0f;

    float massInput = 1.0f;
    float velocityInput[2] = { 0.0f, 0.0f };
    float colorInput[3] = { 1.0f, 1.0f, 1.0f };

    char idInput[256] = "CelestialBody";

    CelestialBody* selectedBody = nullptr;


    unsigned int gridVAO, gridVBO;
    
    AppState() : window(nullptr), gridVAO(0), gridVBO(0) {}
};


#endif