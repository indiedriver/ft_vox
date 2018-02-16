#include <iomanip>
#include <list>
#include "camera.hpp"
#include "chunk.hpp"
#include "env.hpp"
#include "renderer.hpp"
#include "shader.hpp"

std::string float_to_string(float f, int prec) {
  std::ostringstream out;
  out << std::setprecision(prec) << std::fixed << f;
  return out.str();
}

void print_debug_info(Renderer &renderer, Camera &camera) {
  float fheight = static_cast<float>(renderer.getScreenHeight());
  float fwidth = static_cast<float>(renderer.getScreenWidth());
  renderer.renderText(10.0f, fheight - 20.0f, 0.35f,
                      "X: " + float_to_string(camera.pos.x, 2) +
                          " Y: " + float_to_string(camera.pos.y, 2) +
                          " Z: " + float_to_string(camera.pos.z, 2),
                      glm::vec3(1.0f, 1.0f, 1.0f));
}

int main(int argc, char **argv) {
  Env env(1280, 720);
  ChunkManager chunkManager;
  Renderer renderer(env.width, env.height);
  Camera camera(glm::vec3(0.0, 70.0, 1.0), glm::vec3(0.0, 70.0, 0.0), env.width,
                env.height);
  bool wireframe = false;
  bool debugMode = false;
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  while (!glfwWindowShouldClose(env.window)) {
    env.update();
    chunkManager.update(camera.pos);
    chunkManager.setRenderAttributes(renderer);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwPollEvents();
    camera.queryInput(env.inputHandler.keys, env.inputHandler.mousex,
                      env.inputHandler.mousey);
    camera.update();
    renderer.uniforms.view = camera.view;
    renderer.uniforms.proj = camera.proj;
    renderer.draw();
    renderer.flush();
    if (debugMode) {
      print_debug_info(renderer, camera);
    }
    glfwSwapBuffers(env.window);
    GL_DUMP_ERROR("draw loop");
    if (env.inputHandler.keys[GLFW_KEY_ESCAPE]) {
      glfwSetWindowShouldClose(env.window, 1);
    }
    if (env.inputHandler.keys[GLFW_KEY_M]) {
      env.inputHandler.keys[GLFW_KEY_M] = false;
      wireframe = !wireframe;
      wireframe ? glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
                : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (env.inputHandler.keys[GLFW_KEY_I]) {
      env.inputHandler.keys[GLFW_KEY_I] = false;
      debugMode = !debugMode;
    }
  }
}
