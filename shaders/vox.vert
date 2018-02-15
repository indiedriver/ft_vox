#version 410 core
layout (location = 0) in vec4 vert_pos; //xyz = pos, w = face siding

uniform mat4 MVP;

flat out vec3 frag_normal;

const vec3 normal_table[6] = vec3[6](
  vec3(0.0, 0.0, 1.0),
  vec3(0.0, 0.0, -1.0),
  vec3(1.0, 0.0, 0.0),
  vec3(-1.0, 0.0, 0.0),
  vec3(0.0, -1.0, 0.0),
  vec3(0.0, 1.0, 0.0)
);

void main() {
  frag_normal = normal_table[uint(vert_pos.w)];
  gl_Position = MVP * vec4(vert_pos.xyz, 1.0);
}
