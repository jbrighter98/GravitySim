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


enum ClickMode { FREE_PLACE, ANALYZE, ORBITAL_PLACE };
ClickMode currentMode = FREE_PLACE;


double mouseX = 0.0;
double mouseY = 0.0;
bool g_leftMouseButtonPressed = false;
bool startLeftPress = false;

bool isPaused = false;

Camera camera;

void processInput(GLFWwindow* window, Camera& cam, float dt) {
    float speed = 2.0f * cam.zoom * dt; // Scale speed by zoom so it feels consistent

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cam.position.y += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cam.position.y -= speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cam.position.x -= speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cam.position.x += speed;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomSpeed = 0.1f;
    
    if (yoffset > 0) {
        camera.zoom *= (1.0f - zoomSpeed); // Zoom in
    } else {
        camera.zoom *= (1.0f + zoomSpeed); // Zoom out
    }

    // --- The Cap ---
    float minZoom = 0.2f; // How close can you get?
    float maxZoom = 10.0f; // How far can you see?
    
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

    //GLFWwindow* window = (GLFWwindow*)arg;

    // Input handling
    if (glfwGetKey(state->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        std::cout << "Exiting..." << std::endl;
        glfwSetWindowShouldClose(state->window, true);
    }

    // Rendering commands
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Deep space blue/black
    glClear(GL_COLOR_BUFFER_BIT);


    float currentFrame = glfwGetTime();
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
        strncpy(selectedID, state->selectedBody->ID, 255);
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


    // Click to add Body
    if (g_leftMouseButtonPressed) {
        
        if(ImGui::GetIO().WantCaptureMouse) {
            startLeftPress = true;
        }
        else if(!startLeftPress){

            if(currentMode == FREE_PLACE) {

                char newID[256];
                // Initialize newID with the base idInput
                strncpy(newID, state->idInput, 255);
                newID[255] = '\0';

                // If the ID already exists, append a suffix
                if(checkIDExists(state, newID)) {
                    int count = 1;
                    while(true) {
                        char tempID[256];
                        snprintf(tempID, sizeof(tempID), "%s_%d", state->idInput, count);
                        std::cout << "ID already exists. Trying: " << tempID << std::endl;
                        if(!checkIDExists(state, tempID)) {
                            strncpy(newID, tempID, 255);
                            newID[255] = '\0';
                            break;
                        }
                        count++;
                    }
                }

                std::cout << "Final ID for new body: " << newID << std::endl;

                CelestialBody newBody(newID, glm::vec2(worldX, worldY), state->massInput, 0.05f * sqrt(state->massInput), glm::vec4(state->colorInput[0], state->colorInput[1], state->colorInput[2], 1.0f));
                newBody.velocity = glm::vec2(state->velocityInput[0], state->velocityInput[1]);

                state->bodies.push_back(newBody);

                std::cout << "Added Body at: " << worldX << ", " << worldY << std::endl;
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
        }
    }


    for(auto& body : state->bodies) {

        if (body.isDebris) {
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


    ///*** UI INPUTS START *** /

    // Start ImGui Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Define the Menu (at the bottom)
    ImGui::SetNextWindowPos(ImVec2(10, height - 120), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width - 20, 130));

    ImGui::Begin("Simulation Controls", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // ID input
    ImGui::InputText( "Name", state->idInput, sizeof(state->idInput) );

    // Mass Input
    ImGui::InputFloat("Mass", &state->massInput, 1.0f, 1.0f);

    // Velocity Inputs
    ImGui::InputFloat2("Initial Velocity (X, Y)", state->velocityInput);

    ImGui::SameLine(); // Put the button on the same line

    // Change the Click Mode button text based on state
    char* clickButtonLabel = "";
    if(currentMode == ANALYZE) {
        clickButtonLabel = "Analyze";
    }
    else if(currentMode == FREE_PLACE) {
        clickButtonLabel = "Free Place";
    }
    else if(currentMode == ORBITAL_PLACE) {
        clickButtonLabel = "Orbital Place";
    }

    if (ImGui::Button(clickButtonLabel)) {
        if(currentMode == ANALYZE) {
            state->selectedBody = nullptr;
            currentMode = FREE_PLACE;
        }
        else if(currentMode == FREE_PLACE) {
            state->selectedBody = nullptr;
            currentMode = ORBITAL_PLACE;
        }
        else if(currentMode == ORBITAL_PLACE) {
            state->selectedBody = nullptr;
            currentMode = ANALYZE;
        }
    }

    ImGui::SameLine(); // Put the button on the same line

    // Clear Button
    if (ImGui::Button("Clear All Bodies")) {
        state->bodies.clear();
        state->selectedBody = nullptr;
    }

    // Color Inputs
    ImGui::ColorEdit3("Body Color", state->colorInput);

    ImGui::End();


    // --- PAUSE / PLAY BUTTON ---
    // Set position to Top-Left
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(200, 50));

    ImGui::Begin("Playback Controls", NULL, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove);

    // Change the button text based on state
    const char* buttonLabel = isPaused ? "[ PAUSED ]" : "[ RUNNING ]";

    if (ImGui::Button(buttonLabel, ImVec2(100, 30))) {
        isPaused = !isPaused;
    }

    if (isPaused) {
        ImGui::SameLine();
        if (ImGui::Button("STEP >", ImVec2(80, 30))) {
            // Run physics for a tiny, fixed amount of time
            updatePhysics(state, 0.016f); 
        }
    }

    ImGui::End();

    if (state->selectedBody && state->selectedBody->exists) {
        ImGui::SetNextWindowPos(ImVec2(width - 260, 10), ImGuiCond_FirstUseEver);
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
    GLFWwindow* window = glfwCreateWindow(1200, 800, "N-Body Gravity Sim", NULL, NULL);
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

    // Initialize the Grid Shader object


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