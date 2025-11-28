#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>

static const int GRID_W = 512;
static const int GRID_H = 512;

GLuint gridTex[2];
GLuint simFBO = 0;
GLuint quadVAO = 0, quadVBO = 0;
GLuint simProgram = 0;
GLuint displayProgram = 0;
int pingIndex = 0;
int pongIndex = 1;

// ---------------- Helpers ----------------

void glfwErrorCallback(int error, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

std::string loadTextFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<char> log(logLen);
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        fprintf(stderr, "Shader compile error:\n%s\n", log.data());
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint compileShaderFromFile(GLenum type, const char* path) {
    std::string src = loadTextFile(path);
    if (src.empty()) return 0;
    const char* csrc = src.c_str();
    return compileShader(type, csrc);
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

void glauberHalfSweep(int parity, float temperature, float randSeed) {
    glUseProgram(simProgram);

    GLint locGrid   = glGetUniformLocation(simProgram, "uGrid");
    GLint locParity = glGetUniformLocation(simProgram, "uParity");
    GLint locTemp   = glGetUniformLocation(simProgram, "uTemperature");
    GLint locSeed   = glGetUniformLocation(simProgram, "uRandSeed");
    GLint locTexel  = glGetUniformLocation(simProgram, "uTexelSize");

    glUniform1i(locGrid, 0);
    glUniform1i(locParity, parity);
    glUniform1f(locTemp, temperature);
    glUniform1f(locSeed, randSeed);

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

void renderGridToScreen(GLFWwindow* window) {
    glUseProgram(displayProgram);

    GLint locGrid = glGetUniformLocation(displayProgram, "uGrid");
    glUniform1i(locGrid, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gridTex[pingIndex]);

    // Use framebuffer size, not window size
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// ---------------- main ----------------

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

    int windowWidth = 800, windowHeight = 800;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight,
                                          "Glauber Ising (GL3 ping-pong)", nullptr, nullptr);
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

    createGridTextures(GRID_W, GRID_H);
    createSimFBO();
    createFullscreenQuad();

    const char* fullscreenVertPath = "shaders/fullscreen.vert";
    const char* simFragPath        = "shaders/sim.frag";
    const char* displayFragPath    = "shaders/display.frag";

    // Simulation program
    GLuint vsFullscreen = compileShaderFromFile(GL_VERTEX_SHADER, fullscreenVertPath);
    GLuint fsSim        = compileShaderFromFile(GL_FRAGMENT_SHADER, simFragPath);
    simProgram          = linkProgram(vsFullscreen, fsSim);
    glDeleteShader(fsSim);

    // Display program
    GLuint fsDisplay    = compileShaderFromFile(GL_FRAGMENT_SHADER, displayFragPath);
    displayProgram      = linkProgram(vsFullscreen, fsDisplay);
    glDeleteShader(fsDisplay);
    // We can reuse the same vertex shader for both and delete after linking
    glDeleteShader(vsFullscreen);

    float temperature = 2.4f;
    int frame = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glauberSweep(temperature, float(frame));

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderGridToScreen(window);

        glfwSwapBuffers(window);
        ++frame;
    }

    glDeleteProgram(simProgram);
    glDeleteProgram(displayProgram);
    glDeleteFramebuffers(1, &simFBO);
    glDeleteTextures(2, gridTex);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &quadVAO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
