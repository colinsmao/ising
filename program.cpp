// main.cpp
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// layout
static const int GRID_W = 512;
static const int GRID_H = 512;
static const float rightFrac  = 0.25f; // right 25% of screen is the info area, including panel
static const float borderFrac = 0.03f; // border width around panel
static const int windowHeight = 512;
static const int windowWidth = (int)(windowHeight * (1 + rightFrac));

// OpenGL objects
GLuint gridTex[2];
GLuint simFBO = 0;
GLuint quadVAO = 0, quadVBO = 0;
GLuint simProgram = 0;
GLuint displayProgram = 0;
GLuint panelProgram = 0;

int pingIndex = 0;
int pongIndex = 1;

// Simulation control
const float gCriticalTemp   = 2.269f;  // 2/log(1+sqrt(2))
const float gCriticalField  = 0.0f;
const float gTempMin        = gCriticalTemp - 2.0f;
const float gTempMax        = gCriticalTemp + 2.0f;
const float gFieldMin       = gCriticalField - 1.0f;
const float gFieldMax       = gCriticalField + 1.0f;

// Panel interaction state (normalized [0,1]^2 in panel space)
float gTemperature    = gCriticalTemp;
float gFieldStrength  = gCriticalField;
float gPanelU         = 0.5f;  // x in panel (temperature)
float gPanelV         = 0.5f;  // y in panel (chemical potential)
bool  gIsDragging     = false;

// cell colors
float gSpinUpColorR = 0.2f;
float gSpinUpColorG = 0.8f;
float gSpinUpColorB = 1.0f;
float gSpinDownColorR = 0.95f;
float gSpinDownColorG = 0.95f;
float gSpinDownColorB = 0.95f;

double lastPrintTime = 0.0;

// =============== Utility ===============

void glfwErrorCallback(int error, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

std::string loadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileShaderFromFile(GLenum type, const char* path) {
    std::string src = loadTextFile(path);
    if (src.empty()) return 0;
    const char* csrc = src.c_str();

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen);
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        fprintf(stderr, "Shader compile error in %s:\n%s\n", path, log.data());
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen);
        glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        fprintf(stderr, "Program link error:\n%s\n", log.data());
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// =============== Simulation buffers ===============

std::vector<float> makeInitialSpins(int W, int H) {
    std::vector<float> data(W * H);
    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < W; ++i) {
            float r = float(rand()) / float(RAND_MAX);
            data[j * W + i] = (r < 0.5f) ? -1.0f : 1.0f;
        }
    }
    return data;
}

void createGridTextures(int W, int H) {
    auto initData = makeInitialSpins(W, H);

    glGenTextures(2, gridTex);
    for (int k = 0; k < 2; ++k) {
        glBindTexture(GL_TEXTURE_2D, gridTex[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F,
                     W, H, 0,
                     GL_RED, GL_FLOAT,
                     (k == 0 ? initData.data() : nullptr));

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void createSimFBO() {
    glGenFramebuffers(1, &simFBO);
}

void createFullscreenQuad() {
    float quadVerts[] = {
            // pos    // uv
            -1.f, -1.f,  0.f, 0.f,
            1.f, -1.f,  1.f, 0.f,
            -1.f,  1.f,  0.f, 1.f,
            1.f,  1.f,  1.f, 1.f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

// =============== Simulation step ===============

void glauberHalfSweep(int parity, float temperature, float randSeed) {
    glUseProgram(simProgram);

    GLint locGrid   = glGetUniformLocation(simProgram, "uGrid");
    GLint locParity = glGetUniformLocation(simProgram, "uParity");
    GLint locTemp   = glGetUniformLocation(simProgram, "uTemperature");
    GLint locSeed   = glGetUniformLocation(simProgram, "uRandSeed");
    GLint locTexel  = glGetUniformLocation(simProgram, "uTexelSize");
    GLint locField     = glGetUniformLocation(simProgram, "uFieldStrength");

    glUniform1i(locGrid, 0);
    glUniform1i(locParity, parity);
    glUniform1f(locTemp, temperature);
    glUniform1f(locSeed, randSeed);
    glUniform1f(locField, gFieldStrength);

    float texelX = 1.0f / float(GRID_W);
    float texelY = 1.0f / float(GRID_H);
    glUniform2f(locTexel, texelX, texelY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gridTex[pingIndex]);

    glBindFramebuffer(GL_FRAMEBUFFER, simFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gridTex[pongIndex], 0);
    GLenum drawBuf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuf);

    glViewport(0, 0, GRID_W, GRID_H);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::swap(pingIndex, pongIndex);
}

void glauberSweep(float temperature, float frameIndex) {
    glauberHalfSweep(0, temperature, frameIndex + 0.0f);
    glauberHalfSweep(1, temperature, frameIndex + 1.0f);
}

// =============== Rendering ===============

void renderSimToLeft(GLFWwindow* window) {
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    int panelW = fbW / 4;
    int simW   = fbW - panelW;

    glUseProgram(displayProgram);
    GLint locGrid = glGetUniformLocation(displayProgram, "uGrid");
    GLint locSpinUpColor = glGetUniformLocation(displayProgram, "uSpinUpColor");
    GLint locSpinDownColor = glGetUniformLocation(displayProgram, "uSpinDownColor");
    glUniform1i(locGrid, 0);
    glUniform3f(locSpinUpColor, gSpinUpColorR, gSpinUpColorG, gSpinUpColorB);
    glUniform3f(locSpinDownColor, gSpinDownColorR, gSpinDownColorG, gSpinDownColorB);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gridTex[pingIndex]);

    glViewport(0, 0, simW, fbH);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void renderPanel(GLFWwindow* window) {
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    float regionW  = fbW * rightFrac;      // width of right-side region in framebuffer pixels
    float regionX0 = fbW - regionW;       // left edge of panel region
    // we'll anchor panel at bottom inside that region

    float panelSize = regionW * (1.0f - 2.0f * borderFrac); // square side length
    int   panelW    = (int)panelSize;
    int   panelH    = (int)panelSize;

    int panelX = (int)(regionX0 + borderFrac * regionW); // move right from regionX0
    int panelY = (int)(borderFrac * regionW);            // border from bottom

    glUseProgram(panelProgram);

    GLint locDot   = glGetUniformLocation(panelProgram, "uDot");
    GLint locUp    = glGetUniformLocation(panelProgram, "uSpinUpColor");
    GLint locDown  = glGetUniformLocation(panelProgram, "uSpinDownColor");

    glUniform2f(locDot, gPanelU, gPanelV);
    glUniform3f(locUp,   gSpinUpColorR,   gSpinUpColorG,   gSpinUpColorB);
    glUniform3f(locDown, gSpinDownColorR, gSpinDownColorG, gSpinDownColorB);

    // panel viewport is smaller than the right region → black border around it
    glViewport(panelX, panelY, panelW, panelH);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // render ImGui on top
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui uses window coords
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);
    float rightPanelX0 = winW * (1.0f - rightFrac);
    float margin = winW * rightFrac * borderFrac;
    ImGui::SetNextWindowPos(ImVec2(rightPanelX0 + margin, margin), ImGuiCond_Always);

    ImGui::Begin("Ising State",
                 nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoMove);

    ImGui::Text("T/Tc = % 0.3f", gTemperature / gCriticalTemp);
    ImGui::Text("   h = %+0.3f", gFieldStrength);

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


// =============== Mouse interaction for panel ===============

// Convert cursor position to panel coords, update gPanelU/gPanelV + temperature/chemical potential
void updatePanelFromCursor(GLFWwindow* window, double x, double y) {
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    double regionW  = winW * rightFrac;
    double regionX0 = winW - regionW;

    double panelSize = regionW * (1.0 - 2.0 * borderFrac);

    double panelX0 = regionX0 + borderFrac * regionW;
    double panelX1 = panelX0 + panelSize;

    // In framebuffer, panelY (bottom) = borderFrac * regionW
    double panelBottomGL = borderFrac * regionW;
    double panelTopGL    = panelBottomGL + panelSize;

    // Convert to window coords (0 at top, increasing downward):
    double panelBottomWin = winH - panelBottomGL; // bottom
    double panelTopWin    = winH - panelTopGL;    // top

    // Clamp cursor to panel rect
    if (x < panelX0) x = panelX0;
    if (x > panelX1) x = panelX1;
    if (y < panelTopWin)    y = panelTopWin;
    if (y > panelBottomWin) y = panelBottomWin;

    // Normalize to [0,1] in panel space
    double u     = (x - panelX0) / (panelX1 - panelX0);      // 0..1 left→right
    double v_raw = (y - panelTopWin) / (panelBottomWin - panelTopWin); // 0..1 top→bottom
    double v     = 1.0 - v_raw;                             // 0 bottom, 1 top

    gPanelU = (float)u;
    gPanelV = (float)v;

    // Map panelU,V -> T and μ (symmetric around critical point)
    gTemperature   = gTempMin  + gPanelU * (gTempMax  - gTempMin);
    gFieldStrength = gFieldMin + gPanelV * (gFieldMax - gFieldMin);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);

            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);

            double regionW  = winW * rightFrac;
            double regionX0 = winW - regionW;

            double panelSize = regionW * (1.0 - 2.0 * borderFrac);

            double panelX0 = regionX0 + borderFrac * regionW;
            double panelX1 = panelX0 + panelSize;

            double panelBottomGL = borderFrac * regionW;
            double panelTopGL    = panelBottomGL + panelSize;

            double panelBottomWin = winH - panelBottomGL;
            double panelTopWin    = winH - panelTopGL;

            if (x >= panelX0 && x <= panelX1 &&
                y >= panelTopWin && y <= panelBottomWin) {
                gIsDragging = true;
                updatePanelFromCursor(window, x, y);
            }
        } else if (action == GLFW_RELEASE) {
            gIsDragging = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    if (gIsDragging) {
        updatePanelFromCursor(window, x, y);
    }
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mode) {
    // when a user presses the escape key, we set the WindowShouldClose property to true, closing the application
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// =============== main ===============

int main() {
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight,
                                          "Glauber Ising", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return -1;
    }

    printf("GL Version: %s\n", glGetString(GL_VERSION));

    // IMGUI init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Mouse callbacks
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetKeyCallback(window, keyCallback);

    createGridTextures(GRID_W, GRID_H);
    createSimFBO();
    createFullscreenQuad();

    // Build programs from shader files
    const char* fullscreenVertPath = "shaders/fullscreen.vert";
    const char* simFragPath        = "shaders/sim.frag";
    const char* displayFragPath    = "shaders/display.frag";
    const char* panelFragPath      = "shaders/panel.frag";

    GLuint vsFullscreen = compileShaderFromFile(GL_VERTEX_SHADER, fullscreenVertPath);
    GLuint fsSim        = compileShaderFromFile(GL_FRAGMENT_SHADER, simFragPath);
    simProgram          = linkProgram(vsFullscreen, fsSim);
    glDeleteShader(fsSim);

    GLuint fsDisplay    = compileShaderFromFile(GL_FRAGMENT_SHADER, displayFragPath);
    displayProgram      = linkProgram(vsFullscreen, fsDisplay);
    glDeleteShader(fsDisplay);

    GLuint fsPanel      = compileShaderFromFile(GL_FRAGMENT_SHADER, panelFragPath);
    panelProgram        = linkProgram(vsFullscreen, fsPanel);
    glDeleteShader(fsPanel);

    glDeleteShader(vsFullscreen);

    int frame = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Run one Glauber sweep per frame with current temperature
        glauberSweep(gTemperature, float(frame));

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderSimToLeft(window);
        renderPanel(window);

        glfwSwapBuffers(window);
        ++frame;
    }

    glDeleteProgram(simProgram);
    glDeleteProgram(displayProgram);
    glDeleteProgram(panelProgram);
    glDeleteFramebuffers(1, &simFBO);
    glDeleteTextures(2, gridTex);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &quadVAO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
