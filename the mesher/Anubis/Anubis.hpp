#pragma once
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "iostream"
#include "AnubisMath.hpp"
#include <cinttypes>
#include <vector>
namespace MEMORY_READING {
    typedef enum {
        VBO_FREE_MEMORY_ATI = 0x87FB,
        TEXTURE_FREE_MEMORY_ATI = 0x87FC,
        RENDERBUFFER_FREE_MEMORY_ATI = 0x87FD
    } EXTENSION_MEMORY_SPACE;
}
namespace KEY {
    typedef enum {
        KEY_NULL = 0,
        APOSTROPHE = 39,     // '
        COMMA = 44,          // ,
        MINUS = 45,          // -
        PERIOD = 46,         // .
        SLASH = 47,          // /
        ZERO = 48,
        ONE = 49,
        TWO = 50,
        THREE = 51,
        FOUR = 52,
        FIVE = 53,
        SIX = 54,
        SEVEN = 55,
        EIGHT = 56,
        NINE = 57,
        SEMICOLON = 59,      // ;
        EQUAL = 61,          // =
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        // Function keys
        SPACE = 32,
        ESCAPE = 256,
        ENTER = 257,
        TAB = 258,
        BACKSPACE = 259,
        INSERT = 260,
        DELETE = 261,
        RIGHT = 262,
        LEFT = 263,
        DOWN = 264,
        UP = 265,
        PAGE_UP = 266,
        PAGE_DOWN = 267,
        HOME = 268,
        END = 269,
        CAPS_LOCK = 280,
        SCROLL_LOCK = 281,
        NUM_LOCK = 282,
        PRINT_SCREEN = 283,
        PAUSE = 284,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
        LEFT_SHIFT = 340,
        LEFT_CONTROL = 341,
        LEFT_ALT = 342,
        LEFT_SUPER = 343,    // Windows/Command key
        RIGHT_SHIFT = 344,
        RIGHT_CONTROL = 345,
        RIGHT_ALT = 346,
        RIGHT_SUPER = 347,   // Windows/Command key
        KB_MENU = 348,       // Menu key
    } KEYS;
    
}
namespace GL_EXT {
    typedef enum {
        GPU_IMAGE_COPY = GL_ARB_copy_image
    } EXTENSIONS;
}
namespace Anubis {

    struct PersistentBuffer {
        GLuint id;
        void* ptr;
        size_t size;

        void Create(GLuint binding, size_t size) {
            this->size = size;
            glGenBuffers(1, &id);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
            GLbitfield flags = GL_MAP_WRITE_BIT |
                GL_MAP_PERSISTENT_BIT |
                GL_MAP_COHERENT_BIT |
                GL_DYNAMIC_STORAGE_BIT;

            glBufferStorage(GL_SHADER_STORAGE_BUFFER, size, nullptr, flags);
            ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, size, flags);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id);
        }
    };

    float cubeVertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

         // Front face
         -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
          0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

          // Left face
          -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
          -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,

          // Right face
           0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
           0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,

           // Bottom face
           -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
            0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,

            // Top face
            -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
             0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
    };

    const Math::Vector3 up = { 0,1,0 };
    class Quad {
    private:
        float vertices[24] = {
            // positions       // colors
            -1.0f, -1.0f, 0.0f, 1,0,0,
             1.0f, -1.0f, 0.0f, 0,1,0,
             1.0f,  1.0f, 0.0f, 0,0,1,
            -1.0f,  1.0f, 0.0f, 1,1,1
        };
        unsigned int indices[6] = { 0,1,2, 2,3,0 };
    public:
        unsigned int VBO, VAO, EBO;

        void Create(float off=0) {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
        }
        //need vulkan alternative
        void Draw() {
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        //need vulkan alternative
        ~Quad() {
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
        }
        //need vulkan alternative
    };

    std::vector<float> GenerateSphere(float radius, int stacks, int sectors) {
        std::vector<float> verts;

        float pi = 3.14159265359f;

        for (int i = 0; i < stacks; ++i) {
            float phi1 = pi * i / stacks;
            float phi2 = pi * (i + 1) / stacks;

            for (int j = 0; j < sectors; ++j) {
                float theta1 = 2 * pi * j / sectors;
                float theta2 = 2 * pi * (j + 1) / sectors;
                float x1 = radius * sin(phi1) * cos(theta1);
                float y1 = radius * cos(phi1);
                float z1 = radius * sin(phi1) * sin(theta1);
                float x2 = radius * sin(phi2) * cos(theta1);
                float y2 = radius * cos(phi2);
                float z2 = radius * sin(phi2) * sin(theta1);
                float x3 = radius * sin(phi2) * cos(theta2);
                float y3 = radius * cos(phi2);
                float z3 = radius * sin(phi2) * sin(theta2);
                float x4 = radius * sin(phi1) * cos(theta2);
                float y4 = radius * cos(phi1);
                float z4 = radius * sin(phi1) * sin(theta2);
                verts.insert(verts.end(), { x1,y1,z1,  x2,y2,z2,  x3,y3,z3 });
                verts.insert(verts.end(), { x1,y1,z1,  x3,y3,z3,  x4,y4,z4 });
            }
        }

        return verts;
    }
    class HWVIrGeomMesh {
    public:
        unsigned int VAO = 0, VBO = 0, colorVBO = 0;
        int vertexCount = 0;
        bool hasColor = false;

        void Create(float* verts, size_t floatCount) {
            vertexCount = (int)(floatCount / 3);

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), verts, GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glBindVertexArray(0);
        }

        void CreateWithColor(float* verts, size_t floatCount,
            float* colors, size_t colorFloatCount,
            GLenum usage = GL_STATIC_DRAW)
        {
            vertexCount = (int)(floatCount / 3);

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &colorVBO);
            glBindVertexArray(VAO);

            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, floatCount * sizeof(float), verts, usage);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
            glBufferData(GL_ARRAY_BUFFER, colorFloatCount * sizeof(float), colors, usage);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);

            hasColor = true;
            glBindVertexArray(0);
        }

        void Draw() {
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
            glBindVertexArray(0);
        }

        ~HWVIrGeomMesh() { Delete(); }

        void Delete() {
            if (colorVBO) { glDeleteBuffers(1, &colorVBO); colorVBO = 0; }
            if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
            if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
            hasColor = false;
        }
    };
    HWVIrGeomMesh* CreateCubeMesh() {
        float cubeVertices[] = {
            // Back face
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

             // Front face
             -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
              0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

              // Left face
              -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
              -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,

              // Right face
              0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
               0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,

               // Bottom face
               -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
                0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,

                // Top face
                -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
                 0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
        };

        HWVIrGeomMesh* cubeMesh = new HWVIrGeomMesh();

        size_t totalElements = sizeof(cubeVertices) / sizeof(float);

        cubeMesh->Create(cubeVertices, totalElements);

        return cubeMesh;
    }
    class Window {
    private:
        GLFWwindow* window = nullptr;
        double lastTime = 0.0;
        int frameCount = 0;
        float cachedFPS = 0.0f;

    public:
        Math::Vector2 size;

        inline void Init(int width = 1280, int height = 720,int major = 4, int minor = 6,const char* caption = "caption") {
            if (!glfwInit()) {
                std::cerr << "Failed to initialize GLFW\n";
                return;
            }
            size.x = (float)width;
            size.y = (float)height;

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_RESIZABLE, false);
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

            window = glfwCreateWindow(width, height, caption, nullptr, nullptr);
            if (!window) {
                std::cerr << "Failed to create GLFW window\n";
                glfwTerminate();
                return;
            }

            glfwMakeContextCurrent(window);
            glfwSwapInterval(0);

            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::cerr << "Failed to initialize GLAD\n";
                return;
            }

            lastTime = glfwGetTime(); 
        }

        inline float getFPS() {
            frameCount++;
            double now = glfwGetTime();
            double delta = now - lastTime;
            if (delta >= 1.0) {
                cachedFPS = (float)(frameCount / delta);
                frameCount = 0;
                lastTime = now;
            }
            return cachedFPS;
        }

        inline Math::Vector2 GetMousePos() {
            double mx = 0.0, my = 0.0;
            glfwGetCursorPos(window, &mx, &my);
            return Math::Vector2{ (float)mx, (float)(size.y - my) };
        }

        inline void DisableCursor() {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        inline void EnableCursor() {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        inline GLFWwindow* GetWindowHandle() {
            return window;
        }

        inline Math::Vector2 GetMousePosition() {
            double mx;
            double my;
            glfwGetCursorPos(window, &mx, &my);
            return Math::Vector2{ mx,my };
        }
        inline void SetMousePosition(Math::Vector2 pos) {
            glfwSetCursorPos(window, pos.x, pos.y);
        }
        inline void SetTitle(std::string txt) {
            glfwSetWindowTitle(window, txt.c_str());
        }
        inline void DrawQuad(const Quad& quad) {
            glBindVertexArray(quad.VAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        //need vulkan alternative
        inline bool GetMouseButtonPressed(int button) {
            return glfwGetMouseButton(window, button) == GLFW_PRESS;
        }
        inline bool GetKeyPressed(int key) {
            return glfwGetKey(window, key) == GLFW_PRESS;
        }
        inline void Clear() {
            glClear(GL_COLOR_BUFFER_BIT);
        }
        inline void Resize(int width, int height) {
            if (!window) return;
            size.x = static_cast<float>(width);
            size.y = static_cast<float>(height);
            glfwSetWindowSize(window, width, height);
            glViewport(0, 0, width, height);
        }
        inline bool IsOpen() {
            return !glfwWindowShouldClose(window);
        }

        inline void UpdateBuffer() {
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        inline void Close() {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    };
    class Camera {
    public:
        Math::Vector3 position;
        Math::Vector3 target;
        bool taggedIn = false;
        inline Math::Vector3 GetTarget() {
            return Math::SubtractVector3(target, position);
        }
        inline Math::Vector3 GetRight() {
            Math::Vector3 forward = GetForward();
            Math::Vector3 right = Math::NormalizeVector3(Math::CrossVector3(forward, up)); // Reverse the order
            return right;
        }
        inline Math::Vector3 GetForward() {
            return Math::NormalizeVector3(Math::SubtractVector3(target, position));
        }
        inline void Pitch(float angle) {
            Math::Vector3 targetd = Math::SubtractVector3(this->target, position);
            Math::Vector3 forward = Math::NormalizeVector3(Math::SubtractVector3(targetd, position));
            Math::Vector3 right = Math::NormalizeVector3(Math::CrossVector3(up, forward));
            this->target = Math::RotateVector3(targetd, right, angle);
        }
        inline void Yaw(float angle) {
            Math::Vector3 targeted = Math::SubtractVector3(target, position);
            targeted = Math::RotateVector3(targeted, up, angle);
            this->target = Math::AddVector3(position, targeted);

        }
        inline bool UpdateRPG(Math::Vector2 mousePos, Math::Vector2 oldMouse, Window window) {

            if (window.GetKeyPressed(KEY::W)) position.x += 5;
            if (window.GetKeyPressed(KEY::S)) position.x -= 5;
            if (window.GetKeyPressed(KEY::D)) position.z += 5;
            if (window.GetKeyPressed(KEY::A)) position.z -= 5;


        }
        inline bool UpdateFREE(Math::Vector2 mousePos, Math::Vector2 oldMouse, Window window) {
            Math::Vector2 mouseDelta;
            mouseDelta.x = mousePos.x - oldMouse.x;
            mouseDelta.y = mousePos.y - oldMouse.y;
            double speed = (5+(window.GetKeyPressed(KEY::LEFT_SHIFT))*100) * 0.01;

            double sensitivity = 0.1;
            if (fabs(mouseDelta.x) < 0.01f) mouseDelta.x = 0.0f;
            if (fabs(mouseDelta.y) < 0.01f) mouseDelta.y = 0.0f;
            mouseDelta.x *= sensitivity;
            mouseDelta.y *= sensitivity;

            static double yaw = 0.0f;
            static double pitch = 0.0f;

            yaw += mouseDelta.x;
            pitch += mouseDelta.y;

            pitch = Math::Clamp(pitch, -89.0f, 89.0f);

            Math::Vector3 cameraFront;
            cameraFront.x = cos(Math::Radians(yaw)) * cos(Math::Radians(pitch));
            cameraFront.y = sin(Math::Radians(pitch));
            cameraFront.z = sin(Math::Radians(yaw)) * cos(Math::Radians(pitch));
            cameraFront = Math::NormalizeVector3(cameraFront);

            this->target = Math::AddVector3(position, cameraFront);
            if (window.GetKeyPressed(KEY::DOWN)) Pitch(speed);
            if (window.GetKeyPressed(KEY::UP)) Pitch(-speed);
            if (window.GetKeyPressed(KEY::RIGHT)) Yaw(speed);
            if (window.GetKeyPressed(KEY::LEFT)) Yaw(-speed);
            if (window.GetKeyPressed(KEY::W)) position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(GetForward(), speed));
            if (window.GetKeyPressed(KEY::S)) position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(GetForward(), -speed));
            if (window.GetKeyPressed(KEY::D)) position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(GetRight(), -speed));
            if (window.GetKeyPressed(KEY::A)) position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(GetRight(), speed));

            return false;
        }
        inline bool UpdateFPS(Math::Vector2 mousePos, Math::Vector2 oldMouse, Window window) {
            bool output = false;
            Math::Vector2 mouseDelta;
            mouseDelta.x = mousePos.x - oldMouse.x;
            mouseDelta.y = mousePos.y - oldMouse.y;

            if (mousePos.x != oldMouse.x || mousePos.y != oldMouse.y) {
                output = true;
            }
            double speed = (10 + (window.GetKeyPressed(KEY::LEFT_SHIFT)) * 50) * 0.01;

            double sensitivity = 0.1;
            if (fabs(mouseDelta.x) < 0.01f) mouseDelta.x = 0.0f;
            if (fabs(mouseDelta.y) < 0.01f) mouseDelta.y = 0.0f;
            mouseDelta.x *= -sensitivity;
            mouseDelta.y *= -sensitivity;

            static double yaw = 0.0f;
            static double pitch = 0.0f;

            yaw += mouseDelta.x;
            pitch += mouseDelta.y;

            pitch = Math::Clamp(pitch, -89.0f, 89.0f);

            Math::Vector3 cameraFront;
            cameraFront.x = cos(Math::Radians(yaw)) * cos(Math::Radians(pitch));
            cameraFront.y = sin(Math::Radians(pitch));
            cameraFront.z = sin(Math::Radians(yaw)) * cos(Math::Radians(pitch));
            cameraFront = Math::NormalizeVector3(cameraFront);

            this->target = Math::AddVector3(position, cameraFront);
            if (window.GetKeyPressed(KEY::DOWN)) { Pitch(speed); Pitch(speed); output = true; };
            if (window.GetKeyPressed(KEY::UP)) { Pitch(-speed); Pitch(-speed); output = true; }
            if (window.GetKeyPressed(KEY::RIGHT)) { Yaw(speed); Yaw(speed); output = true; }
            if (window.GetKeyPressed(KEY::LEFT)) { Yaw(-speed); Yaw(-speed); output = true; }
            if (window.GetKeyPressed(KEY::W)) {
                Math::Vector3 forward = GetForward();
                forward.y = 0.0f; // prevent vertical movement
                forward = Math::NormalizeVector3(forward);
                position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(forward, speed));
                output = true;
            }

            if (window.GetKeyPressed(KEY::S)) {
                Math::Vector3 forward = GetForward();
                forward.y = 0.0f; // prevent vertical movement
                forward = Math::NormalizeVector3(forward);
                position = Math::AddVector3(position, Math::MultiplyVector3ByFloat(forward, -speed));
                output = true;
            }
            return output;
        }
    };

    void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam) {
        std::cerr << "[GL DEBUG] " << message << "\n";
    }

    //need vulkan alternative and renaming
    void DebugMode() {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, nullptr);
    }
    //need vulkan alternative
}