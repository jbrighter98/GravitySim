#ifndef PHYSICS_H
#define PHYSICS_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <iostream>
#include <string>
#include <cstring>

#include "Globals.hpp"


void handleCollisions(AppState* state, CelestialBody& a, CelestialBody& b, std::vector<CelestialBody>& newDebris ) {

    if(!b.exists) {
        return;
    }
    if(!a.exists) {
        return;
    }


    // Calculate Combined Mass
    float m1 = a.mass;
    float m2 = b.mass;
    float combinedMass = m1 + m2;

    // calculate relative velocity and reduced mass
    // then calculate totalKE from there

    glm::vec2 vRel = a.velocity - b.velocity;
    float vRelSq = glm::length2(vRel);

    float reducedMass = (m1 * m2) / (m1 + m2);

    float totalKE = 0.5f * reducedMass * vRelSq;

    std::cout << "Collision detected!" << std::endl;
    std::cout << "Total KE: " << totalKE << std::endl;


    float energyThreshold = 1.0f;
    if(m1 >= m2) {
        energyThreshold = (float)((3 * state->G * std::pow(m1, 2)) / (5 * a.radius));
    }
    else {
        energyThreshold = (float)((3 * state->G * std::pow(m2, 2)) / (5 * b.radius));
        // Update name if B is more massive, since it will be the "survivor"
#ifdef __EMSCRIPTEN__
        std::strncpy(a.ID, b.ID, 255);
#else
        strncpy_s(a.ID, b.ID, 255);
#endif
        a.ID[255] = '\0';
    }
    

    // merge color by mass
    glm::vec3 colorA = glm::vec3(a.color);
    glm::vec3 colorB = glm::vec3(b.color);

    glm::vec3 blendedRGB = (colorA * m1 + colorB * m2) / combinedMass;
    a.color = glm::vec4(blendedRGB, 1.0f);

    // Center of Mass Position
    a.position = (a.position * m1 + b.position * m2) / combinedMass;

    if(totalKE > energyThreshold) {

        // --- HIGH ENERGY COLLISION: SHATTER ---

        std::cout << "HIGH energy collision!" << std::endl;

        int particleCount = static_cast<int>((a.mass + b.mass) / 2.0f) + (rand() % 10); // Proportional to energy?
        float debrisMassRatio = 0.2f; // % of mass becomes debris
        float debrisMassTotal = combinedMass * debrisMassRatio;
        float massPerParticle = debrisMassTotal / particleCount;

        a.mass = combinedMass - debrisMassTotal;

        // Calculate collision Normal
        glm::vec2 normal = glm::normalize(b.position - a.position);
        // Find contact point
        glm::vec2 contactPoint = a.position + (normal * a.radius);

        // Determine the "Base" ejection direction
        // If m is small, debris splashes "back" towards the impactor
        float impactAngle = atan2(normal.y, normal.x);


        // Calculate the Bias (Max 20 degrees)
        // If m2 is smaller, bias moves towards B. If m1 is smaller, bias moves towards A.
        float massFactor = (a.mass - b.mass) / (a.mass + b.mass);
        float biasOffset = massFactor * glm::radians(10.0f);

        // Eject Particles

        for (int p = 0; p < particleCount; ++p) {
            // Decide: Top Jet or Bottom Jet?
            float side = (p % 2 == 0) ? 1.0f : -1.0f;
            
            // Base is 90 degrees (1.57 rad) or -90 degrees
            float jetCenter = impactAngle + (side * glm::radians(90.0f)) + biasOffset;
            
            // Spread window of 20 degrees (+/- 10 degrees)
            float variation = ((rand() % 100 / 100.0f) - 0.5f) * glm::radians(20.0f);
            float finalAngle = jetCenter + variation;
            
            glm::vec2 ejectionDir(cos(finalAngle), sin(finalAngle));

            // Calculate offset to prevent immediate collision upon spawning
            float offsetDistance = a.radius + 0.05f;
            glm::vec2 spawnPos = contactPoint + (ejectionDir * offsetDistance);

            // Velocity proportional to impact energy
            float speed = sqrt(totalKE / combinedMass) * (1.5f + (rand() % 100 / 100.0f)); 
            
            CelestialBody debris("Debris", spawnPos, massPerParticle, 0.01f, a.color, true);
            debris.velocity = a.velocity + (ejectionDir * speed);
            debris.color = a.color; // Inherit parent color
            
            newDebris.push_back(debris); // Add to the simulation
        }

        a.velocity = (a.velocity * m1 + b.velocity * m2) / combinedMass;

        a.radius = 0.05f * sqrt(a.mass);
    }
    else {

        // --- LOW ENERGY COLLISION: MERGE ---

        std::cout << "LOW energy collision!" << std::endl;

        // Conservation of Momentum: (m1v1 + m2v2) / (m1+m2)
        a.velocity = (a.velocity * m1 + b.velocity * m2) / combinedMass;

        a.mass = combinedMass;

        a.radius = 0.05f * sqrt(a.mass);
    }

    a.isDebris = false;

    if(state->selectedBody == &b || state->selectedBody == &a) {
        state->selectedBody = &a; // Focus camera on the survivor
    }

    b.exists = false; 

}


bool isOverlapping(const CelestialBody& a, const CelestialBody& b) {
    // We assume circles have a radius property. 
    // If they don't, you can use a constant like 0.1f for now.
    float radiusA = a.radius; 
    float radiusB = b.radius;

    glm::vec2 delta = a.position - b.position;
    float distSq = glm::dot(delta, delta); // x^2 + y^2
    float radiusSum = radiusA + radiusB;

    return distSq < (radiusSum * radiusSum);
}


void updatePhysics(AppState* state, float deltaTime) {

    // Calculate Forces/Acceleration

    std::vector<CelestialBody> debris;

    for (size_t i = 0; i < state->bodies.size(); ++i) {
        glm::vec2 totalForce(0.0f);

        for (size_t j = 0; j < state->bodies.size(); ++j) {
            
            if (i == j) continue; // Don't pull yourself!

            if (state->bodies[i].isDebris && state->bodies[j].isDebris) continue; // don't let debris interact with other debris (for performance).


            if (isOverlapping(state->bodies[i], state->bodies[j])) {
                // Handle collision here!
                handleCollisions(state, state->bodies[i], state->bodies[j], debris);
            }

            glm::vec2 direction = state->bodies[j].position - state->bodies[i].position;
            float distanceSq = glm::dot(direction, direction); // r^2
            
            // Softening factor to prevent infinite force when bodies overlap
            float softening = 0.01f; 
            float forceMagnitude = state->G * (state->bodies[i].mass * state->bodies[j].mass) / (distanceSq + softening);

            totalForce += glm::normalize(direction) * forceMagnitude;
        }
        // F = ma -> a = F/m. (If mass is 1, acceleration = force)
        state->bodies[i].acceleration = totalForce / state->bodies[i].mass;
    }

    if(!debris.empty()){
        
        // Save selectedBody's ID before potential reallocation
        char selectedID[256] = "";
        if (state->selectedBody) {
#ifdef __EMSCRIPTEN__
            std::strncpy(selectedID, state->selectedBody->ID, 255);
#else
            strncpy_s(selectedID, state->selectedBody->ID, 255);
#endif
            selectedID[255] = '\0';
        }

        state->bodies.insert(state->bodies.end(), debris.begin(), debris.end());

        // Re-find selectedBody by ID after vector reallocation
        state->selectedBody = nullptr;
        if (selectedID[0] != '\0') {
            for (auto& body : state->bodies) {
                if (std::strcmp(body.ID, selectedID) == 0) {
                    state->selectedBody = &body;
                    break;
                }
            }
        }
    }
    

    // Integration (Move the bodies)
    for (auto& body : state->bodies) {
        body.velocity += body.acceleration * deltaTime;
        body.position += body.velocity * deltaTime;
    }
}


#endif