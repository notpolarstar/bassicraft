#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    float yaw;
    float pitch;
    float speed;
    float speed_turn;
    float sensitivity;
    float fov;
};

class Player {
public:
    Camera camera;
    //bool keys[1024];
    bool firstMouse;
    float lastX;
    float lastY;

    Player() {
        camera = {
            .pos = glm::vec3(0.0f, 0.0f, 0.2f),
            .front = glm::vec3(0.0f, 0.0f, -1.0f),
            .up = glm::vec3(0.0f, -1.0f, 0.0f),
            .right = glm::vec3(1.0f, 0.0f, 0.0f),
            .worldUp = glm::vec3(0.0f, -1.0f, 0.0f),
            .yaw = -90.0f,
            .pitch = 0.0f,
            .speed = 0.55f,
            .speed_turn = 0.1f,
            .sensitivity = 0.1f,
            .fov = 45.0f
        };
        firstMouse = true;
        lastX = 0.0f;
        lastY = 0.0f;
    }

    void processInput(GLFWwindow* window) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera.pos += camera.speed * camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camera.pos -= camera.speed * camera.front;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.pos -= glm::normalize(glm::cross(camera.front, camera.up)) * camera.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.pos += glm::normalize(glm::cross(camera.front, camera.up)) * camera.speed;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            camera.pos += camera.speed * camera.up;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            camera.pos -= camera.speed * camera.up;
        }
        mouse_movement(window);
    }

    void mouse_movement(GLFWwindow* window) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        xoffset *= camera.sensitivity;
        yoffset *= camera.sensitivity;

        camera.yaw += xoffset;
        camera.pitch += yoffset;

        if (camera.pitch > 89.0f) {
            camera.pitch = 89.0f;
        }
        if (camera.pitch < -89.0f) {
            camera.pitch = -89.0f;
        }

        glm::vec3 front;
        front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
        front.y = sin(glm::radians(camera.pitch));
        front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
        camera.front = glm::normalize(front);
    }
};
