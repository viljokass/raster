#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <glm/glm.hpp>

struct Face_m {
  int v1v;
  int v2v;
  int v3v;
} Face_default = {0, 0, 0};
typedef struct Face_m Face;

class Model {
public:
  std::vector<glm::vec3> vertices;
  std::vector<glm::vec3> work_vertices;

  std::vector<glm::vec3> normals;
  std::vector<glm::vec3> work_normals;

  std::vector<Face> faces;
  std::vector<glm::ivec3> colours;

  std::vector<glm::vec2> texcoords;

  glm::ivec3 colour;

  bool textures = false;
  bool iscolour = false;

  Model(std::string filename) {

    Face work_face;
    glm::vec3 nc1;
    glm::vec3 nc2;

    std::ifstream file(filename);
    std::string buffer;

    float x_off = 0.0;
    float y_off = 0.0; 
    float z_off = 0.0;

    float f1, f2, f3;
    std::string s, s1, s2, s3;

    unsigned int vs = 0;
    unsigned int ns = 0;
    unsigned int fs = 0;
    unsigned int ts = 0;

    while (std::getline(file, buffer)) {
      std::stringstream ss(buffer);
      ss >> s;
      if (s == "v") {
        // Parse vertices
        ss >> f1 >> f2 >> f3;
        vertices.push_back(glm::vec3(f1, f2, f3));
        ++vs;
      }
      else if (s == "f") {
        // Parse faces
        ss >> s1 >> s2 >> s3;
        work_face.v1v = stoi(s1);
        work_face.v2v = stoi(s2);
        work_face.v3v = stoi(s3);
        faces.push_back(work_face);
        ++fs;
      }
      else if (s == "vt") {
        ss >> f1 >> f2;
        texcoords.push_back(glm::vec2(f1, f2));
        ++ts;
      }
      else if (s == "c") {
        ss >> s1 >> s2 >> s3;
        colour.x = stoi(s1);
        colour.y = stoi(s2);
        colour.z = stoi(s3);
        iscolour = true;
      }
      else if (s == "p") {
        ss >> x_off >> y_off >> z_off;
      }
    }
    if (ts > 0) textures = true;

    for (unsigned int i = 0; i < vs; ++i) {
      nc1 = vertices[i];
      vertices[i] = glm::vec3(nc1.x + x_off, nc1.y + y_off, nc1.z + z_off);
      work_vertices.push_back(glm::vec3(0.0));
    }

    for(unsigned int i = 0; i < fs; ++i) {
      colours.push_back(glm::ivec3(std::rand()%255, std::rand()%255, std::rand()%255));
      work_face = faces[i];
      nc1 = vertices[work_face.v3v - 1] - vertices[work_face.v1v - 1];
      nc2 = vertices[work_face.v2v - 1] - vertices[work_face.v1v - 1];
      normals.push_back(glm::cross(nc2, nc1));
    }
    for (unsigned int i = 0; i < fs; ++i) {
      work_normals.push_back(glm::vec3(0.0));
    }
  }
};
