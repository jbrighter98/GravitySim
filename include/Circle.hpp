#ifndef CIRCLE_H
#define CIRCLE_H

#include <vector>
#include <cmath>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Circle {
public:
    unsigned int VAO, VBO;
    int vertexCount;

    Circle(float radius, int segments = 36) {
        std::vector<float> vertices;
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);

        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * 3.1415926f * float(i) / float(segments);
            vertices.push_back(radius * cosf(theta));
            vertices.push_back(radius * sinf(theta));
            vertices.push_back(0.0f);
        }
        vertexCount = vertices.size() / 3;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    void draw() {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
    }
};
#endif