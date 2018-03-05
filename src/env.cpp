#include "env.hpp"

Env::Env() : Env(0, 0) {}

Env::Env(unsigned short width, unsigned short height)
    : width(width), height(height), _frame(0) {
  if (!glfwInit()) return;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  if (width == 0 && height == 0) {
    GLFWmonitor *primary_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(primary_monitor);
    this->width = mode->width;
    this->height = mode->height;
    window = glfwCreateWindow(this->width, this->height, "ft_vox",
                              primary_monitor, NULL);
  } else {
    window = glfwCreateWindow(width, height, "ft_vox", NULL, NULL);
  }
  if (!window) {
    std::cout << "Could not create window\n";
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize OpenGL context" << std::endl;
  }
  std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
  std::cout << "GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

  inputHandler.screenWidth = width;
  inputHandler.screenHeight = height;
  inputHandler.mouseDisabled = false;

  glfwSetWindowUserPointer(window, &inputHandler);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetMouseButtonCallback(window, mouseKeyCallback);
  glfwSwapInterval(0);

  glEnable(GL_DEBUG_OUTPUT);
  while (glGetError() != GL_NO_ERROR)
    ;  // Flush gl_error
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

Env::~Env() { glfwTerminate(); }

void Env::update() {
  updateFpsCounter();
  static double previousTime = glfwGetTime();
  static int frame_count;
  double currentTime = glfwGetTime();
  double deltaTime = currentTime - previousTime;
  previousTime = currentTime;
  this->_deltaTime = static_cast<float>(deltaTime);
  this->_absoluteTime = static_cast<float>(currentTime);
  this->_frame++;
}

void Env::updateFpsCounter() {
  static double previous_seconds = glfwGetTime();
  static int frame_count;
  double current_seconds = glfwGetTime();
  double elapsed_seconds = current_seconds - previous_seconds;
  if (elapsed_seconds > 0.25) {
    previous_seconds = current_seconds;
    this->_fps =
        static_cast<float>(frame_count) / static_cast<float>(elapsed_seconds);
    std::ostringstream tmp;
    tmp << _fps << " fps";
    glfwSetWindowTitle(window, tmp.str().c_str());
    frame_count = 0;
  }
  frame_count++;
}

float Env::getDeltaTime() { return (this->_deltaTime); }

float Env::getAbsoluteTime() { return (this->_absoluteTime); }

float Env::getFrame() { return (this->_frame); }

float Env::getFPS() { return (this->_fps); }

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mode) {
  (void)scancode;
  (void)mode;
  InputHandler *inputHandler =
      reinterpret_cast<InputHandler *>(glfwGetWindowUserPointer(window));
  if (action == GLFW_PRESS) {
    inputHandler->keys[key] = true;
  } else if (action == GLFW_RELEASE) {
    inputHandler->keys[key] = false;
  }
}

void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  InputHandler *inputHandler =
      reinterpret_cast<InputHandler *>(glfwGetWindowUserPointer(window));
  inputHandler->mousex = static_cast<float>(xpos);
  inputHandler->mousey = static_cast<float>(ypos);
}

void mouseKeyCallback(GLFWwindow *window, int button, int action, int mods) {
  (void)mods;
  InputHandler *inputHandler =
      reinterpret_cast<InputHandler *>(glfwGetWindowUserPointer(window));
  if (action == GLFW_PRESS) {
    inputHandler->mouse_keys[button] = true;
  } else if (action == GLFW_RELEASE) {
    inputHandler->mouse_keys[button] = false;
  }
}
