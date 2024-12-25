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

    glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    bool is_jumping = false;
    bool ghost_mode = false;

    int selected_item = 1;

    Player() {
        camera = {
            .pos = glm::vec3(0.0f, 0.0f, 0.2f),
            .front = glm::vec3(0.0f, 0.0f, -1.0f),
            .up = glm::vec3(0.0f, -1.0f, 0.0f),
            .right = glm::vec3(-1.0f, 0.0f, 0.0f),
            .worldUp = glm::vec3(0.0f, -1.0f, 0.0f),
            .yaw = -90.0f,
            .pitch = 0.0f,
            .speed = 0.05f,
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
        if (camera.yaw > 360.0f) {
            camera.yaw = 0.0f;
        }
        if (camera.yaw < 0.0f) {
            camera.yaw = 360.0f;
        }

        glm::vec3 front;
        front.x = cos(glm::radians(camera.yaw)) * -cos(glm::radians(camera.pitch));
        front.y = sin(glm::radians(-camera.pitch));
        front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
        camera.front = glm::normalize(front);
    }

    void update_mouse_pos(GLFWwindow* window) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    }
};
