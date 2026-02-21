#define GLM_ENABLE_EXPERIMENTAL
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif
#include <glm/gtx/dual_quaternion.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.hpp"
#include "Circle.hpp"
#include "Camera.hpp"
#include "Physics.hpp"
#include "Globals.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/gtx/string_cast.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


std::string VERSION = "1.0.0";


enum ClickMode { FREE_PLACE, ANALYZE, ORBITAL_PLACE };
ClickMode currentMode = FREE_PLACE;


double mouseX = 0.0;
double mouseY = 0.0;
bool g_leftMouseButtonPressed = false;
bool g_rightMouseButtonPressed = false;
bool startLeftPress = false;
bool startRightPress = false;

bool isPaused = false;
bool debrisFade = true;

bool clockwiseOrbit = true;

Camera camera;

void processInput(GLFWwindow* window, Camera& cam, float dt) {
    float speed = 2.0f * cam.zoom * dt; // Scale speed by zoom so it feels consistent

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.position.y += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.position.y -= speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.position.x -= speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.position.x += speed;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return; // ImGui handles this scroll; don't zoom the camera
    }
    
    float zoomSpeed = 0.1f;
    
    if (yoffset > 0) {
        camera.zoom *= (1.0f - zoomSpeed); // Zoom in
    } else {
        camera.zoom *= (1.0f + zoomSpeed); // Zoom out
    }

    // --- The Cap ---
    float minZoom = 0.2f; // How close can you get?
    float maxZoom = 20.0f; // How far can you see?
    
    camera.zoom = glm::clamp(camera.zoom, minZoom, maxZoom);
}

// Callback function to handle cursor position updates
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    // xpos and ypos are relative to the top-left corner of the window
    mouseX = xpos;
    mouseY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        g_leftMouseButtonPressed = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        g_leftMouseButtonPressed = false;
        startLeftPress = false;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        
        g_rightMouseButtonPressed = true;
    
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        g_rightMouseButtonPressed = false;
        startRightPress = false;
    }
}


// Callback to resize the viewport if the user drags the window corner
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}


bool checkIDExists(AppState* state, const char* id) {
    for(auto& body : state->bodies) {
        if(strcmp(body.ID, id) == 0) {
            return true;
        }
    }
    return false;
}

void DrawBorder(AppState* state, glm::vec3 color, float thickness) {
    state->myShader->use();
    
    // Identity matrices mean "don't move it, just use NDC coordinates (-1 to 1)"
    glm::mat4 identity = glm::mat4(1.0f);
    state->myShader->setMat4("view", identity);
    state->myShader->setMat4("projection", identity);
    state->myShader->setMat4("model", identity);
    state->myShader->setVec4("uColor", glm::vec4(color, 1.0f));

    glLineWidth(thickness); 
    glBindVertexArray(state->borderVAO);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}


void validateID(AppState* state, char (&newID)[256]) {

    // Initialize newID with the base idInput
#ifdef __EMSCRIPTEN__
    std::strncpy(newID, state->idInput, 255);
#else
    strncpy_s(newID, state->idInput, 255);
#endif
    newID[255] = '\0';

    // If the ID already exists, append a suffix
    if(checkIDExists(state, newID)) {
        std::cout << "ID " << newID << " already exists" << std::endl;
        int count = 1;
        while(true) {
            char tempID[256];
            snprintf(tempID, sizeof(tempID), "%s_%d", state->idInput, count);
            if(!checkIDExists(state, tempID)) {
#ifdef __EMSCRIPTEN__
                std::strncpy(newID, tempID, 255);
#else
                strncpy_s(newID, tempID, 255);
#endif
                newID[255] = '\0';
                break;
            }
            count++;
        }
    }
}













// Main rendering and update function called every frame


void UpdateFrame(void* arg) {
    if(!arg){
        std::cout << "Error: AppState pointer is null!" << std::endl;
        return;
    }

    AppState* state = static_cast<AppState*>(arg);

    if(!state->window) {
        std::cout << "Error: GLFWwindow pointer in AppState is null!" << std::endl;
        return;
    }

    // Input handling
#ifndef __EMSCRIPTEN__
    // Check for Escape key to close the window only in native builds (since Emscripten doesn't support this)
    if (glfwGetKey(state->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        std::cout << "Exiting..." << std::endl;
        glfwSetWindowShouldClose(state->window, true);
    }
#endif

    // Rendering commands
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Deep space blue/black
    glClear(GL_COLOR_BUFFER_BIT);


    float currentFrame = (float)glfwGetTime();
    float deltaTime = currentFrame - state->lastFrame;
    state->lastFrame = currentFrame;

    if (deltaTime > 0.1f) {
        deltaTime = 0.1f; 
    }

    float simulationDT = isPaused ? 0.0f : deltaTime;

    state->myShader->use();

    // Create a projection (Standard 2D orthographic)
    int width, height;
    glfwGetFramebufferSize(state->window, &width, &height);

    // Calculate aspect ratio (float division is important!)
    float aspectRatio = (float)width / (float)height;

    processInput(state->window, camera, deltaTime);

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(aspectRatio);


    state->gridShader->use();
    state->gridShader->setMat4("invView", glm::inverse(camera.getViewMatrix()));
    state->gridShader->setMat4("invProj", glm::inverse(camera.getProjectionMatrix((float)width/height)));

    // Send resolution manually (since we haven't added setVec2 to the class yet)
    glUniform2f(glGetUniformLocation(state->gridShader->ID, "u_resolution"), (float)width, (float)height);

    // Actually Draw the square
    glBindVertexArray(state->gridVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);


    state->myShader->use();
    state->myShader->setMat4("view", view);
    state->myShader->setMat4("projection", projection);

    glBindVertexArray(state->bodyShape->VAO);

    updatePhysics(state, simulationDT);

    char selectedID[256] = "";
    if (state->selectedBody) {
#ifdef __EMSCRIPTEN__
        strncpy(selectedID, state->selectedBody->ID, 255);
#else
        strncpy_s(selectedID, state->selectedBody->ID, 255);
#endif
        selectedID[255] = '\0';
    }

    // remove non-existant bodies
    state->bodies.erase(std::remove_if(state->bodies.begin(), state->bodies.end(), 
            [](const CelestialBody& b) { return !b.exists; }), 
            state->bodies.end());


    state->selectedBody = nullptr;
    // Re-validate selectedBody pointer after vector modification
    if (selectedID[0] != '\0') {  // If we had a selected body
        for (auto& body : state->bodies) {
            if (strcmp(body.ID, selectedID) == 0) {
                state->selectedBody = &body;
                break;
            }
        }
    }


    // Create an almost-transparent version of the body to be placed at the mouse location

    double mouseXNDC = (mouseX / (width / 2.0)) - 1.0;
    double mouseYNDC = 1.0 - (mouseY / (height / 2.0)); // Y is inverted in window coords

    // Account for the Aspect Ratio and Zoom 
    // This matches the math inside camera.getProjectionMatrix
    float worldX = (float)mouseXNDC * aspectRatio * camera.zoom;
    float worldY = (float)mouseYNDC * camera.zoom;

    worldX += camera.position.x;
    worldY += camera.position.y;

    if (state->selectedBody && state->selectedBody->exists) {
        camera.position = state->selectedBody->position;
    }

    if(currentMode == FREE_PLACE) {
        glm::mat4 modelMouse = glm::mat4(1.0f);

        modelMouse = glm::translate(modelMouse, glm::vec3(worldX, worldY, 0.0f));
        modelMouse = glm::scale(modelMouse, glm::vec3(0.05f * sqrt(state->massInput), 0.05f * sqrt(state->massInput), 1.0f));
        
        state->myShader->setMat4("model", modelMouse);

        glm::vec4 renderColor = glm::vec4(state->colorInput[0], state->colorInput[1], state->colorInput[2], 0.2f);

        state->myShader->setVec4("uColor", renderColor);

        state->bodyShape->draw();
    }


    if (currentMode == ORBITAL_PLACE && state->isPlacingOrbit && state->orbitalAnchor) {
        float r = glm::distance(glm::vec2(worldX, worldY), state->orbitalAnchor->position);
        float x = worldX;
        float y = worldY;
        if(r <= state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f) {
            float d = (state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f) - r;
            float angle = atan2(worldY - state->orbitalAnchor->position.y, worldX - state->orbitalAnchor->position.x);
            x = worldX + cos(angle) * d;
            y = worldY + sin(angle) * d;
            r = state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f; // Prevent placing inside the anchor
        }
        // Draw the Ring
        // ring should be just 2 
        state->myShader->use();
        glm::mat4 ringModel = glm::translate(glm::mat4(1.0f), glm::vec3(state->orbitalAnchor->position, 0.0f));
        ringModel = glm::scale(ringModel, glm::vec3(r, r, 1.0f));
        state->myShader->setMat4("model", ringModel);

        glm::vec4 renderColor = glm::vec4(state->colorInput[0], state->colorInput[1], state->colorInput[2], 0.3f);

        state->myShader->setVec4("uColor", renderColor); // Faint color for the ring
        state->bodyShape->drawLines();


        // Draw the Ghost Planet at mouse position with limited radius
        glm::mat4 modelMouse = glm::mat4(1.0f);

        modelMouse = glm::translate(modelMouse, glm::vec3(x, y, 0.0f));
        modelMouse = glm::scale(modelMouse, glm::vec3(0.05f * sqrt(state->massInput), 0.05f * sqrt(state->massInput), 1.0f));
        
        state->myShader->setMat4("model", modelMouse);

        state->myShader->setVec4("uColor", renderColor);

        state->bodyShape->draw();
    }


    // right-click to remove clicked body
    if (g_rightMouseButtonPressed) {

         // right click to remove clicked body
        if(ImGui::GetIO().WantCaptureMouse) {
            startRightPress = true;
        }
        else if(!startRightPress){

            for (auto& body : state->bodies) {
                float radius = body.radius * 1.2f; // Click area is slightly larger than the body
                if (glm::distance2(glm::vec2(worldX, worldY), body.position) < radius * radius) {
                    body.exists = false; // Mark for deletion
                    std::cout << "Removed body: " << body.ID << std::endl;
                    break; // Only remove one body per click
                }
            }

            startRightPress = true;
        }

    }



    // Click to add Body
    if (g_leftMouseButtonPressed) {
        
        if(ImGui::GetIO().WantCaptureMouse) {
            startLeftPress = true;
        }
        else if(!startLeftPress){

            if(currentMode == FREE_PLACE) {

                char newID[256];
                validateID(state, newID);

                CelestialBody newBody(newID, glm::vec2(worldX, worldY), state->massInput, 0.05f * sqrt(state->massInput), glm::vec4(state->colorInput[0], state->colorInput[1], state->colorInput[2], 1.0f));
                newBody.velocity = glm::vec2(state->velocityInput[0], state->velocityInput[1]);

                state->bodies.push_back(newBody);

                std::cout << "Added Body at: " << worldX << ", " << worldY << std::endl;
                std::cout << "ID: " << newID << std::endl;
                std::cout << "Mass: " << state->massInput << std::endl;
                std::cout << "Radius: " << 0.05f * sqrt(state->massInput) << std::endl;
                std::cout << "Initial Velocity: " << newBody.velocity.x << ", " << newBody.velocity.y << std::endl;
                std::cout << "Object Color: " << newBody.color[0] << ", " << newBody.color[1] << ", " << newBody.color[2] << std::endl;

                startLeftPress = true;
            }
            else if(currentMode == ANALYZE) {
                state->selectedBody = nullptr; // Clear previous selection
    
                for (auto& body : state->bodies) {
                    if (body.isDebris) continue; // Ignore dust

                    float dist = glm::distance(glm::vec2(worldX, worldY), body.position);
                    
                    // If the click is inside the planet's radius (with a little extra 'click padding')
                    if (dist < body.radius + 0.05f) {
                        state->selectedBody = &body;
                        break;
                    }
                }
                startLeftPress = true;
            }
            else if(currentMode == ORBITAL_PLACE) {

                if(!state->isPlacingOrbit) {
                    state->orbitalAnchor = nullptr; // Clear previous anchor

                    for (auto& body : state->bodies) {
                        float dist = glm::distance(glm::vec2(worldX, worldY), body.position);
                        if (dist < body.radius + 0.05f) {
                            state->orbitalAnchor = &body;
                            state->isPlacingOrbit = true;
                            isPaused = true; // Pause simulation while placing orbit
                            break;
                        }
                    }
                }
                else if(state->isPlacingOrbit) {

                    char newID[256];
                    validateID(state, newID);

                    float r = glm::distance(glm::vec2(worldX, worldY), state->orbitalAnchor->position);
                    float x = worldX;
                    float y = worldY;
                    if(r <= state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f) {
                        float d = (state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f) - r;
                        float angle = atan2(worldY - state->orbitalAnchor->position.y, worldX - state->orbitalAnchor->position.x);
                        x = worldX + cos(angle) * d;
                        y = worldY + sin(angle) * d;
                        r = state->orbitalAnchor->radius + (0.05f * sqrt(state->massInput)) + 0.05f; // Prevent placing inside the anchor
                    }
                    float vMag = sqrt((state->G * state->orbitalAnchor->mass) / r);
                    
                    glm::vec2 radialDir = glm::normalize(glm::vec2(worldX, worldY) - state->orbitalAnchor->position);
                    if(clockwiseOrbit) {
                        radialDir = -radialDir; // Flip direction for counter-clockwise
                    }
                    glm::vec2 velocity = (glm::vec2(-radialDir.y, radialDir.x) * vMag) + state->orbitalAnchor->velocity; // Add anchor's velocity for moving bodies

                    // Create the body

                    CelestialBody newBody(newID, glm::vec2(x, y), state->massInput, 0.05f * sqrt(state->massInput), glm::vec4(state->colorInput[0], state->colorInput[1], state->colorInput[2], 1.0f));
                    newBody.velocity = velocity;

                    std::cout << "Added Body at: " << x << ", " << y << std::endl;
                    std::cout << "Orbiting around: " << state->orbitalAnchor->ID << std::endl;
                    std::cout << "ID: " << newID << std::endl;
                    std::cout << "Mass: " << state->massInput << std::endl;
                    std::cout << "Radius: " << 0.05f * sqrt(state->massInput) << std::endl;
                    std::cout << "Initial Velocity: " << newBody.velocity.x << ", " << newBody.velocity.y << std::endl;
                    std::cout << "Object Color: " << newBody.color[0] << ", " << newBody.color[1] << ", " << newBody.color[2] << std::endl;

                    
                    state->bodies.push_back(newBody);

                    // Reset state
                    state->isPlacingOrbit = false;
                    state->orbitalAnchor = nullptr;
                    isPaused = false; // Resume the simulation
                }
                startLeftPress = true;
            }



        }
    }


    for(auto& body : state->bodies) {

        if (body.isDebris && debrisFade) {
            body.lifeTime -= body.decaySpeed * simulationDT;

            // If it's fully faded, mark it for removal
            if (body.lifeTime <= 0.0f) {
                body.exists = false;
            }
        }

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, glm::vec3(body.position, 0.0f));
        model = glm::scale(model, glm::vec3(body.radius, body.radius, 1.0f));
        
        state->myShader->setMat4("model", model);

        glm::vec4 renderColor = body.color;
        if (body.isDebris) {
            // Fade the alpha based on remaining life
            renderColor.a *= body.lifeTime; 
        }

        state->myShader->setVec4("uColor", renderColor);

        state->bodyShape->draw();
    }

    if(isPaused) {
        DrawBorder(state, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
    }
    else {
        DrawBorder(state, glm::vec3(0.1f, 0.1f, 0.1f), 1.0f);
    }
    


    ///*** UI INPUTS START *** /

    // Start ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    // --- CLICK CONTROLS BEGIN ---

    ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300));

    ImGui::Begin("Simulation Controls", NULL, ImGuiWindowFlags_NoResize);

    if (ImGui::Button("Analyze")) {
        state->selectedBody = nullptr;
        state->isPlacingOrbit = false; // Reset any ongoing orbital placement
        state->orbitalAnchor = nullptr; // Clear any previous anchor
        currentMode = ANALYZE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Free Place")) {
        state->selectedBody = nullptr;
        state->isPlacingOrbit = false; // Reset any ongoing orbital placement
        state->orbitalAnchor = nullptr; // Clear any previous anchor
        currentMode = FREE_PLACE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Orbital Place")) {
        state->selectedBody = nullptr;
        state->isPlacingOrbit = false; // Reset any ongoing orbital placement
        state->orbitalAnchor = nullptr; // Clear any previous anchor
        currentMode = ORBITAL_PLACE;
    }

    if(currentMode == ANALYZE) {
        ImGui::TextWrapped("Analyze Mode");
        ImGui::TextWrapped("Click on a body to analyze its properties.");
    }
    else if(currentMode == FREE_PLACE) {
        ImGui::TextWrapped("Free Place Mode");
        ImGui::TextWrapped("Click anywhere to place a new body with the specified properties below.");

        ImGui::InputText( "Name", state->idInput, sizeof(state->idInput) );
        ImGui::InputFloat("Mass", &state->massInput, 1.0f, 1.0f);

        ImGui::InputFloat2("V0 (X, Y)", state->velocityInput);

        ImGui::ColorEdit3("Body Color", state->colorInput);
    }
    else if(currentMode == ORBITAL_PLACE) {
        ImGui::TextWrapped("Orbital Place Mode");
        ImGui::TextWrapped("Click an existing body to place a new body in a circular orbit around it.");

        ImGui::InputText( "Name", state->idInput, sizeof(state->idInput) );
        ImGui::InputFloat("Mass", &state->massInput, 1.0f, 1.0f);

        ImGui::ColorEdit3("Body Color", state->colorInput);

        const char* dirButtonLabel = clockwiseOrbit ? "CLOCKWISE" : "COUNTER-CLOCKWISE";
        if (ImGui::Button(dirButtonLabel, ImVec2(200, 30))) {
            clockwiseOrbit = !clockwiseOrbit;
        }
    }

    ImGui::End();

    // --- CLICK CONTROLS END ---

    // --- BODIES LIST BEGIN ---

    ImGui::SetNextWindowPos(ImVec2(10, 380), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200));

    ImGui::Begin("Current Bodies", NULL, ImGuiWindowFlags_AlwaysAutoResize); 

    if (ImGui::BeginListBox("##BodyList", ImVec2(-FLT_MIN, 0.0f))) {
        
        if(state->bodies.empty()) {
            ImGui::TextWrapped("No bodies in the simulation.");
        }
        else {

            for (auto& body : state->bodies) {

                if(body.isDebris) continue; // Skip debris in the list

                const bool isSelected = (state->selectedBody && strcmp(state->selectedBody->ID, body.ID) == 0);

                // ImGui::Selectable() returns true if the item is clicked
                if (ImGui::Selectable(body.ID, isSelected))
                {
                    currentMode = ANALYZE; // Switch to Analyze mode when a body is selected from the list
                    state->selectedBody = &body; // Update the selected body pointer
                }
                
            }
        }
        
        
        ImGui::EndListBox();
    }

    ImGui::End();

    // --- BODIES LIST END ---

    // --- GLOBAL SETTINGS BEGIN ---

    // Set position to Top-Left
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(500, 50));

    ImGui::Begin("Playback Controls", NULL, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove);

    // Change the button text based on state
    const char* buttonLabel = isPaused ? "[ PAUSED ]" : "[ RUNNING ]";

    if (ImGui::Button(buttonLabel, ImVec2(100, 30))) {
        isPaused = !isPaused;
        state->isPlacingOrbit = false; // Reset any ongoing orbital placement
        state->orbitalAnchor = nullptr; // Clear any previous anchor
    }

    if (isPaused) {
        ImGui::SameLine();
        if (ImGui::Button("STEP >", ImVec2(80, 30))) {
            // Run physics for a tiny, fixed amount of time
            updatePhysics(state, 0.016f); 
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear All Bodies", ImVec2(150, 30))) {
        state->selectedBody = nullptr;
        state->isPlacingOrbit = false;
        state->orbitalAnchor = nullptr;
        state->bodies.clear();
        std::cout << "Cleared all bodies from the simulation." << std::endl;
    }

    ImGui::SameLine();

    if (ImGui::Checkbox("Debris Fade", &debrisFade))
    {
        std::cout << "Debris Fade: " << debrisFade << std::endl;
    }

    // Create input for G (Gravitational Constant)
    ImGui::SameLine();
    ImGui::Text("|   G:");
    ImGui::SameLine();
    ImGui::InputFloat("G", &state->G, 0.01f, 0.01f, "%.3f");

    ImGui::End();

    // --- GLOBAL SETTINGS END ---
    

    if (state->selectedBody && state->selectedBody->exists) {
        ImGui::SetNextWindowPos(ImVec2((float)(width - 260), 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(250, 200));

        ImGui::Begin("Body Analysis", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Name: %s", state->selectedBody->ID);
        ImGui::Text("Mass: %.2f", state->selectedBody->mass);
        ImGui::Text("Radius: %.3f", state->selectedBody->radius);
        
        ImGui::Separator();
        
        ImGui::Text("Pos: (%.2f, %.2f)", state->selectedBody->position.x, state->selectedBody->position.y);
        ImGui::Text("Vel: (%.2f, %.2f)", state->selectedBody->velocity.x, state->selectedBody->velocity.y);
        
        // Calculate Speed for the user
        float speed = glm::length(state->selectedBody->velocity);
        ImGui::Text("Total Speed: %.2f", speed);

        if (ImGui::Button("Close Analysis")) {
            state->selectedBody = nullptr;
        }

        ImGui::End();
    } else {
        state->selectedBody = nullptr; // Safety if body was destroyed
    }

    // Rendering ImGui (Call this AFTER drawing your planets)
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ///*** UI INPUTS END *** /

    // Swap buffers and poll IO events
    glfwSwapBuffers(state->window);
    glfwPollEvents();
}

















// Main function: Initialize everything and start the main loop

int main() {
    // Initialize GLFW
    if (!glfwInit()) return -1;

    // Tell GLFW we want to use OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create Window
    GLFWwindow* window = glfwCreateWindow(1420, 800, ("N-Body Gravity Sim v" + VERSION).c_str(), NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Load OpenGL function pointers via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    // Define 2 triangles that make a square covering the whole screen
    float gridVertices[] = {
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f
    };

    // Generate and bind buffers
    unsigned int gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gridVertices), &gridVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Set up Pause border

    float margin = 0.995f; // Pulls the border slightly inward
    float borderVertices[] = {
        -margin,  margin, 0.0f, // Top Left
        -margin, -margin, 0.0f, // Bottom Left
        margin, -margin, 0.0f, // Bottom Right
        margin,  margin, 0.0f  // Top Right
    };

    unsigned int borderVAO, borderVBO;
    glGenVertexArrays(1, &borderVAO);
    glGenBuffers(1, &borderVBO);
    glBindVertexArray(borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, borderVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(borderVertices), borderVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Set up mouse callbacks

    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetScrollCallback(window, scroll_callback);


    // Initialize IMGUI
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    #ifdef __EMSCRIPTEN__
        ImGui_ImplOpenGL3_Init("#version 300 es");
    #else
        ImGui_ImplOpenGL3_Init("#version 460");
    #endif

    // Setup style
    ImGui::StyleColorsDark();


    AppState* state = new AppState();
    state->window = window;
    state->myShader = std::make_unique<Shader>("shaders/vertex.glsl", "shaders/fragment.glsl");
    state->gridShader = std::make_unique<Shader>("shaders/grid_vertex.glsl", "shaders/grid_fragment.glsl");
    state->bodyShape = std::make_unique<Circle>(1.0f, 36); // Unit circle
    state->gridVAO = gridVAO;
    state->gridVBO = gridVBO;
    state->borderVAO = borderVAO;
    state->borderVBO = borderVBO;


    std::cout << "N-Body Gravity Sim" << std::endl;
    std::cout << "Version: " << VERSION << std::endl;

    /*** MAIN LOOP BEGIN ***/


    #ifdef __EMSCRIPTEN__
        // 0 = browser decides frame rate (usually 60fps), 1 = simulate infinite loop
        emscripten_set_main_loop_arg(UpdateFrame, state, 0, 1);
    #else
        while (!glfwWindowShouldClose(state->window)) {
            UpdateFrame(state);
        }

        std::cout << "Cleaning up and exiting..." << std::endl;
        glfwDestroyWindow(state->window);
        glfwTerminate();

    #endif


    /*** MAIN LOOP END ***/


    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}