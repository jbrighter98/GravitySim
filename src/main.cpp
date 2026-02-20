#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/dual_quaternion.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.hpp"
#include "Circle.hpp"
#include "Camera.hpp"
#include "Physics.hpp"

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


double mouseX = 0.0;
double mouseY = 0.0;
bool g_leftMouseButtonPressed = false;
bool startLeftPress = false;

bool isPaused = false;

Camera camera;

enum ClickMode { FREE_PLACE, ANALYZE, ORBITAL_PLACE };
ClickMode currentMode = FREE_PLACE;

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


bool checkIDExists(std::vector<CelestialBody> bodies, const char* id) {
    for(auto& body : bodies) {
        if(strcmp(body.ID, id) == 0) {
            return true;
        }
    }
    return false;
}


int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) return -1;

    // Tell GLFW we want to use OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Create Window
    GLFWwindow* window = glfwCreateWindow(1200, 800, "N-Body Gravity Sim", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. Load OpenGL function pointers via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader myShader("../shaders/vertex.glsl", "../shaders/fragment.glsl");
    Circle bodyShape(1.0f, 32); // Small radius, high detail


    // 1. Define 2 triangles that make a square covering the whole screen
    float gridVertices[] = {
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f
    };

    // 2. Generate and bind buffers
    unsigned int gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gridVertices), &gridVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // 3. Initialize the Grid Shader object
    Shader gridShader("shaders/grid_vertex.glsl", "shaders/grid_fragment.glsl");


    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetScrollCallback(window, scroll_callback);


    std::vector<CelestialBody> bodies; 

    double startx = -1.0f;

    float lastFrame = 0.0f;

    float massInput = 1.0f;
    float velocityInput[2] = { 0.0f, 0.0f };
    float colorInput[3] = { 1.0f, 1.0f, 1.0f };

    //std::string idInput = "CelestialBody";
    char idInput[256] = "CelestialBody";


    // Initialize IMGUI
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460"); // Match your shader version

    // Setup style
    ImGui::StyleColorsDark();

    CelestialBody* selectedBody = nullptr;

    /*** MAIN LOOP BEGIN ***/

    while (!glfwWindowShouldClose(window)) {
        // Input handling
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            std::cout << "Exiting..." << std::endl;
            glfwSetWindowShouldClose(window, true);
        }

        // Rendering commands
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Deep space blue/black
        glClear(GL_COLOR_BUFFER_BIT);


        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (deltaTime > 0.1f) {
            deltaTime = 0.1f; 
        }

        float simulationDT = isPaused ? 0.0f : deltaTime;

        myShader.use();

        // Create a projection (Standard 2D orthographic)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        // Calculate aspect ratio (float division is important!)
        float aspectRatio = (float)width / (float)height;

        processInput(window, camera, deltaTime);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix(aspectRatio);


        gridShader.use();
        gridShader.setMat4("invView", glm::inverse(camera.getViewMatrix()));
        gridShader.setMat4("invProj", glm::inverse(camera.getProjectionMatrix((float)width/height)));

        // 3. Send resolution manually (since we haven't added setVec2 to the class yet)
        glUniform2f(glGetUniformLocation(gridShader.ID, "u_resolution"), (float)width, (float)height);

        // 4. Actually Draw the square
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);


        myShader.use();
        myShader.setMat4("view", view);
        myShader.setMat4("projection", projection);

        glBindVertexArray(bodyShape.VAO);

        updatePhysics(bodies, simulationDT, selectedBody);

        char selectedID[256] = "";
        if (selectedBody) {
            strncpy_s(selectedID, selectedBody->ID, 255);
            selectedID[255] = '\0';
        }

        // remove non-existant bodies
        bodies.erase(std::remove_if(bodies.begin(), bodies.end(), 
             [](const CelestialBody& b) { return !b.exists; }), 
             bodies.end());


        selectedBody = nullptr;
        // Re-validate selectedBody pointer after vector modification
        if (selectedID[0] != '\0') {  // If we had a selected body
            for (auto& body : bodies) {
                if (strcmp(body.ID, selectedID) == 0) {
                    selectedBody = &body;
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

        if (selectedBody && selectedBody->exists) {
            camera.position = selectedBody->position;
        }

        if(currentMode == FREE_PLACE) {
            glm::mat4 modelMouse = glm::mat4(1.0f);

            modelMouse = glm::translate(modelMouse, glm::vec3(worldX, worldY, 0.0f));
            modelMouse = glm::scale(modelMouse, glm::vec3(0.05f * sqrt(massInput), 0.05f * sqrt(massInput), 1.0f));
            
            myShader.setMat4("model", modelMouse);

            glm::vec4 renderColor = glm::vec4(colorInput[0], colorInput[1], colorInput[2], 0.2f);

            myShader.setVec4("uColor", renderColor);

            bodyShape.draw();
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
                    strncpy_s(newID, idInput, 255);
                    newID[255] = '\0';

                    // If the ID already exists, append a suffix
                    if(checkIDExists(bodies, newID)) {
                        int count = 1;
                        while(true) {
                            char tempID[256];
                            snprintf(tempID, sizeof(tempID), "%s_%d", idInput, count);
                            std::cout << "ID already exists. Trying: " << tempID << std::endl;
                            if(!checkIDExists(bodies, tempID)) {
                                strncpy_s(newID, tempID, 255);
                                newID[255] = '\0';
                                break;
                            }
                            count++;
                        }
                    }

                    std::cout << "Final ID for new body: " << newID << std::endl;

                    CelestialBody newBody(newID, glm::vec2(worldX, worldY), massInput, 0.05f * sqrt(massInput), glm::vec4(colorInput[0], colorInput[1], colorInput[2], 1.0f));
                    newBody.velocity = glm::vec2(velocityInput[0], velocityInput[1]);

                    bodies.push_back(newBody);

                    std::cout << "Added Body at: " << worldX << ", " << worldY << std::endl;
                    std::cout << "Object Color: " << newBody.color[0] << ", " << newBody.color[1] << ", " << newBody.color[2] << std::endl;

                    startLeftPress = true;
                }
                else if(currentMode == ANALYZE) {
                    selectedBody = nullptr; // Clear previous selection
        
                    for (auto& body : bodies) {
                        if (body.isDebris) continue; // Ignore dust

                        float dist = glm::distance(glm::vec2(worldX, worldY), body.position);
                        
                        // If the click is inside the planet's radius (with a little extra 'click padding')
                        if (dist < body.radius + 0.05f) {
                            selectedBody = &body;
                            break;
                        }
                    }
                    startLeftPress = true;
                }
            }
        }


        for(auto& body : bodies) {

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
            
            myShader.setMat4("model", model);

            glm::vec4 renderColor = body.color;
            if (body.isDebris) {
                // Fade the alpha based on remaining life
                renderColor.a *= body.lifeTime; 
            }

            myShader.setVec4("uColor", renderColor);

            bodyShape.draw();
        }


        /*** UI INPUTS START ***/

        // 1. Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. Define the Menu (at the bottom)
        ImGui::SetNextWindowPos(ImVec2(10, height - 120), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width - 20, 130));

        ImGui::Begin("Simulation Controls", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        // ID input
        ImGui::InputText( "Name", idInput, sizeof(idInput) );

        // Mass Input
        ImGui::InputFloat("Mass", &massInput, 1.0f, 1.0f);

        // Velocity Inputs
        ImGui::InputFloat2("Initial Velocity (X, Y)", velocityInput);

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
                selectedBody = nullptr;
                currentMode = FREE_PLACE;
            }
            else if(currentMode == FREE_PLACE) {
                selectedBody = nullptr;
                currentMode = ORBITAL_PLACE;
            }
            else if(currentMode == ORBITAL_PLACE) {
                selectedBody = nullptr;
                currentMode = ANALYZE;
            }
        }

        ImGui::SameLine(); // Put the button on the same line

        // Clear Button
        if (ImGui::Button("Clear All Bodies")) {
            bodies.clear();
            selectedBody = nullptr;
        }

        // Color Inputs
        ImGui::ColorEdit3("Body Color", colorInput);

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
                updatePhysics(bodies, 0.016f, selectedBody); 
            }
        }

        ImGui::End();

        if (selectedBody && selectedBody->exists) {
            ImGui::SetNextWindowPos(ImVec2(width - 260, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(250, 200));

            ImGui::Begin("Body Analysis", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Text("Name: %s", selectedBody->ID);
            ImGui::Text("Mass: %.2f", selectedBody->mass);
            ImGui::Text("Radius: %.3f", selectedBody->radius);
            
            ImGui::Separator();
            
            ImGui::Text("Pos: (%.2f, %.2f)", selectedBody->position.x, selectedBody->position.y);
            ImGui::Text("Vel: (%.2f, %.2f)", selectedBody->velocity.x, selectedBody->velocity.y);
            
            // Calculate Speed for the user
            float speed = glm::length(selectedBody->velocity);
            ImGui::Text("Total Speed: %.2f", speed);

            if (ImGui::Button("Close Analysis")) {
                selectedBody = nullptr;
            }

            ImGui::End();
        } else {
            selectedBody = nullptr; // Safety if body was destroyed
        }




        // Rendering ImGui (Call this AFTER drawing your planets)
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /*** UI INPUTS END ***/


        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    /*** MAIN LOOP END ***/


    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}