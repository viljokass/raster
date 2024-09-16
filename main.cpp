#include <gtk/gtk.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "model.h"

const int width = 400 * 2;
const int height = 300 * 2;
const int halfw = 200 * 2;
const int halfh = 150 * 2;
const int scale = 40 * 2;
const float max_float = std::numeric_limits<float>::max();

GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
GtkImage *image;
unsigned int pixel_amount = width * height;

std::vector<Model*> models;

float z_buffer[width*height];

glm::mat4 persview;
glm::vec3 viewpos;
glm::vec3 viewpos_n;

enum Colour {
  RED,
  BLUE,
  ORANGE,
  YELLOW,
  GREEN,
  WHITE,
  GRAY
};

// Parameters for rendering
bool bfc = true;
bool fill = true;
bool enable_z_buffer = true;
bool random_colour = false;
bool textures = false;
Colour col = ORANGE;

// A pixel drawing function.
inline void draw_pixel (int x, int y, unsigned char red, unsigned char green, unsigned char blue) {
  if (y < 1 || y > height - 1) return;
  if (x < 1 || x > width - 1) return;
  *(pixels + (height - y) * rowstride + x * 3) = red;
  *(pixels + (height - y) * rowstride + x * 3 + 1) = green;
  *(pixels + (height - y) * rowstride + x * 3 + 2) = blue;
}

inline void put_in_z_buffer(int x, int y, float value) {
  if (y < 1 || y > height - 1) return;
  if (x < 1 || x > width - 1) return;
  z_buffer[(height - y) * width + x] = value;
}

inline float get_from_z_buffer(int x, int y) {
  if (y < 1 || y > height - 1) return max_float;
  if (x < 1 || x > width - 1) return max_float;
  return z_buffer[(height - y) * width + x];
}

// A line drawing function
void line(int x0, int y0, int x1, int y1, char r, char g, char b) {
  bool steep = false; 
  if (std::abs(x0 - x1) < std::abs(y0 - y1)) {
    std::swap(x0, y0);
    std::swap(x1, y1);
    steep = true;
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  int dx = x1-x0;
  int dy = y1-y0;
  int derr2 = std::abs(dy)*2;
  int err2 = 0;
  int y = y0;
  for (int x = x0; x < x1; ++x) {
    if (steep) {
      draw_pixel(y, x, r, g, b);
    } else {
      draw_pixel(x, y, r, g, b);
    }
    err2 += derr2;
    if (err2 > dx) {
      y += (y1>y0?1:-1);
      err2 -= dx*2;
    }
  }
}

// Draw a wireframe triangle
inline void triangle_w(int p1x, int p1y, int p2x, int p2y, int p3x, int p3y, char r, char g, char b) {
  line(p1x, p1y, p2x, p2y, r, g, b);
  line(p2x, p2y, p3x, p3y, r, g, b);
  line(p3x, p3y, p1x, p1y, r, g, b);
}

// This thing draws triangles
void triangle_f(
  int p1x, int p1y, float p1z, float u1, float v1,
  int p2x, int p2y, float p2z, float u2, float v2,
  int p3x, int p3y, float p3z, float u3, float v3,
  glm::vec3 *normal,
  unsigned char r,
  unsigned char g,
  unsigned char b) {

  // If all triangles are on the same y, stop.
  if (p1y == p2y && p1y == p3y) return;

  // Lighting calcs
  float intensity = std::min(1.0f, std::max(0.05f, glm::dot(*normal, -viewpos_n)));
  r = (unsigned char)(intensity * r);
  g = (unsigned char)(intensity * g);
  b = (unsigned char)(intensity * b);

  // Sort the vertices so that p1 is at the bottom and p3 at the top
  if (p1y > p2y) {
    std::swap(p1x, p2x);
    std::swap(p1y, p2y);
    std::swap(p1z, p2z);
  }
  if (p1y > p3y) {
    std::swap(p1x, p3x);
    std::swap(p1y, p3y);
    std::swap(p1z, p3z);
  }
  if (p2y > p3y) {
    std::swap(p2x, p3x);
    std::swap(p2y, p3y);
    std::swap(p2z, p3z);
  }

  // Raterize the triangle
  //
  // We split the triangle into two parts by p2y.
  // We interpolate x and z using y to
  // determine boundaries for scanline iteration.
  // Then using x we interpolate z to determine depth.
  int total_h = p3y - p1y;
  int segment_h;
  float alpha;
  float beta;
  int xa;
  int xb;
  float z_lerp_const;
  // If z-buffer is enabled
  if (enable_z_buffer) {
    float za;
    float zb;
    float z_com;
    segment_h = p2y - p1y + 1;
    // 1st half
    for (int y = p1y; y <= p2y; ++y) {
      alpha = (float)(y - p1y)/total_h;
      beta  = (float)(y - p1y)/segment_h;
      xa = p1x + (p3x - p1x) * alpha;     // Interpolate starting x
      xb = p1x + (p2x - p1x) * beta;      // Interpolate ending x
      za = p1z + (p3z - p1z) * alpha;     // same for z
      zb = p1z + (p2z - p1z) * beta;
      if (xa > xb) {
        std::swap(xa, xb);
        std::swap(za, zb);
      }
      z_lerp_const = ((zb - za)/(float)(xb - xa));
      for(int x = xa; x <= xb; ++x) {
        // Interpolate z
        z_com = za + (x - xa) * z_lerp_const;
        // Standard z-buffer affair
        if (get_from_z_buffer(x, y) > z_com) {
          put_in_z_buffer(x, y, z_com);
          draw_pixel(x, y, r, g, b);
        }
      }
    }
    // 2nd half
    segment_h = p3y - p2y + 1;
    for (int y = p2y; y <= p3y; ++y) {
      alpha = (float)(y - p1y)/total_h;
      beta  = (float)(y - p2y)/segment_h;
      xa = p1x + (p3x - p1x) * alpha;
      xb = p2x + (p3x - p2x) * beta;
      za = p1z + (p3z - p1z) * alpha;
      zb = p2z + (p3z - p2z) * beta;
      if (xa > xb) {
        std::swap(xa, xb);
        std::swap(za, zb);
      }
      z_lerp_const = ((zb - za)/(float)(xb - xa));
      for (int x = xa; x <= xb; ++x) {
        z_com = za + (x - xa) * z_lerp_const;
        if (get_from_z_buffer(x, y) > z_com) {
          put_in_z_buffer(x, y, z_com);
          draw_pixel(x, y, r, g, b);
        }
      }
    }
  } 
  // If z-buffer is not enabled
  else {
    segment_h = p2y - p1y + 1;
    for (int y = p1y; y <= p2y; ++y) {
      alpha = (float)(y - p1y)/total_h;
      beta  = (float)(y - p1y)/segment_h;
      xa = p1x + (p3x - p1x) * alpha;
      xb = p1x + (p2x - p1x) * beta;
      if (xa > xb) {
        std::swap(xa, xb);
      }
      for(int x = xa; x <= xb; ++x) {
        draw_pixel(x, y, r, g, b);
      } 
    }
    segment_h = p3y - p2y + 1;
    for (int y = p2y; y <= p3y; ++y) {
      alpha = (float)(y - p1y)/total_h;
      beta  = (float)(y - p2y)/segment_h;
      xa = p1x + (p3x - p1x) * alpha;
      xb = p2x + (p3x - p2x) * beta;
      if (xa > xb) {
        std::swap(xa, xb);
      }
      for (int x = xa; x <= xb; ++x) {
        draw_pixel(x, y, r, g, b);
      }
    } 
  }
}

// Clear the screen (rewrite all to certain col)
inline void clear_screen() {
  for (unsigned int x = 0; x < pixel_amount; ++x) {
    *(pixels + x*3) 	  = 20;
    *(pixels + x*3+1) 	= 20;
    *(pixels + x*3+2) 	= 20;
  }
}

// Set all elements of z-buffer to max_float
inline void clear_z_buff() {
  for (unsigned int x = 0; x < pixel_amount; ++x) {
    z_buffer[x] = max_float;
  }
}

// Time variables
static gint64 old_time = 0;
static gint64 delta_time = 0;

// Auxiliary variables for vector
// calculations and drawing
glm::vec3 v;
glm::vec4 wv;

Face draw_face;

glm::ivec3 colour;
glm::ivec3 tmp_colour;

glm::vec3 v1;
glm::vec3 v2;
glm::vec3 v3;
glm::vec3 n;
glm::vec2 uv1;
glm::vec2 uv2;
glm::vec2 uv3;

float wvw;

// Render loop
gboolean render (GtkWidget *widget, GdkFrameClock *clock, gpointer data) {

  // FPS
  gint64 time = gdk_frame_clock_get_frame_time(clock);
  delta_time = time - old_time;
  old_time = time;
  float delta_b = delta_time/1000000.0;
//  std::cout << (int)(1.0/delta_b) << std::endl;
  // Clear the screen and z-buffer
  clear_screen();
  clear_z_buff();

  // Calculate rotation matrix for the model
  glm::mat4 model_mat = glm::mat4(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  );
  model_mat = glm::rotate(model_mat, time/1000000.0f, glm::vec3(0, 1, 0));

  unsigned int mlen = models.size();
  for (unsigned int i = 0; i < mlen; ++i) {
    Model *model = models[i];
    if (model->iscolour) {
      tmp_colour = model->colour;
    }
    else {
      tmp_colour = colour;
    }
    // Transform the vertices here using model and
    // perspective matrices and untransformed vertices
    // Process:
    // 1. put x, y, z values from vertex to work vertex
    // 2. transform work vertex with the matrices
    // 3. perform perspective division
    // 4. put the result to the work vertex array for later
    int vlen = model->vertices.size();
    for(int i = 0; i < vlen; ++i) {
      v = model->vertices[i];
      wv.x = v.x;                   // 1.
      wv.y = v.y;
      wv.z = v.z;
      wv.w = 1.0;
      wv = persview * model_mat * wv;   // 2.
      wvw = wv.w;
      wv.x = wv.x/wvw;              // 3.
      wv.y = wv.y/wvw;
      wv.z = wv.z/wvw;
      model->work_vertices[i].x = wv.x;    // 4.
      model->work_vertices[i].y = wv.y;
      model->work_vertices[i].z = wv.z;
    }
    
    // Transform the normals with model matrix
    int nlen = model->normals.size();
    for (int i = 0; i < nlen; ++i) {
      model->work_normals[i] = glm::normalize(glm::mat3(model_mat) * model->normals[i]);
    }

    // Draw model using faces and transformed vertices
    // Do I need to take the unneccessary-per-cycle checks
    // out or does the compiler already do that when optimizing?
    // As in:
    // for (x = 0, x < 1000) {
    //   if (a) {
    //     [no changes to a]
    //   } else {
    //     [no changes to a]
    //   }
    // }
    // Optimized for performance, the above should sensibly become
    // if (a) {
    //   for (x = 0, x < 1000) {
    //     [no changes to a]
    //   }
    // } else {
    //   for (x = 0, x < 1000) {
    //     [no changes to a]
    //   }
    // }
    int flen = model->faces.size();
    for (int i = 0; i < flen; ++i) {
      draw_face = model->faces[i];
      v1 = model->work_vertices[draw_face.v1v - 1]; // vertex 1
      v2 = model->work_vertices[draw_face.v2v - 1]; // vertex 2
      v3 = model->work_vertices[draw_face.v3v - 1]; // vertex 3
      if (model->textures) {
        uv1 = model->texcoords[draw_face.v1v - 1];
        uv2 = model->texcoords[draw_face.v2v - 1];
        uv3 = model->texcoords[draw_face.v3v - 1];
      }
      // Calculate dot product of the view-space 
      // vertices and view direction
      float dot = 
        glm::dot(
          glm::normalize(glm::cross(v3-v1, v2-v1)), 
          viewpos_n
        );
      // Back face culling (if above dot product is negative)
      if (bfc && dot < 0) continue;
      if (fill) {
        if (random_colour) {
          colour = model->colours[i];
        }
        // Draw the triangle
        triangle_f(
          halfw + (int)(v1.x * scale), halfh + (int)(v1.y * scale), v1.z, uv1.x, uv1.y,
          halfw + (int)(v2.x * scale), halfh + (int)(v2.y * scale), v2.z, uv2.x, uv2.y,
          halfw + (int)(v3.x * scale), halfh + (int)(v3.y * scale), v3.z, uv3.x, uv3.y,
          &model->work_normals[i], tmp_colour.x, tmp_colour.y, tmp_colour.z
        );
      } else {
        // Draw the wireframe triangle
        triangle_w(
          halfw + (int)(v1.x * scale), halfh + (int)(v1.y * scale),
          halfw + (int)(v2.x * scale), halfh + (int)(v2.y * scale),
          halfw + (int)(v3.x * scale), halfh + (int)(v3.y * scale),
          tmp_colour.x, tmp_colour.y, tmp_colour.z
        );
      }
    }
  }
  // Swap the image to the screen and process the next image
  gtk_image_set_from_pixbuf (image, pixbuf);
  return 1;
}

glm::ivec3 choose_colour(Colour colour) {
  switch (colour) {
    case RED:
      return glm::ivec3(250, 0, 0);
    case BLUE:
      return glm::ivec3(0, 0, 250);
    case ORANGE:
      return glm::ivec3(250, 125, 0);
    case YELLOW:
      return glm::ivec3(250, 250, 0);
    case GREEN:
      return glm::ivec3(0, 250, 0);
    case WHITE:
      return glm::ivec3(250);
    case GRAY:
      return glm::ivec3(160);
  }
  return glm::ivec3(250, 125, 0);
}

void print_help() {
  std::cout << "Possible arguments are:" << std::endl;
  std::cout << " * '-help'                    > Displays this message. " << std::endl;
  std::cout << " * '-bfc'                     > Disables back face culling. " << std::endl;
  std::cout << " * '-wire'                    > Draws wireframe triangles (not filled)." << std::endl;
  std::cout << " * '-zbuff'                   > Disables the not-so-good z-buffer." << std::endl;
  std::cout << " * '-f [path/to/file.obj]'    > Loads the specified .obj file." << std::endl;
  std::cout << " * '-c [colour]'              > specifies the object colour." << std::endl;
  std::cout << " * Colours: 'red', 'green', 'blue', 'orange', 'yellow', 'white', 'random', 'gray'." << std::endl << std::endl;
}

int main(int argc, const char* argv[]) {

  // Handle the CLI args 
  bool file = false;
  std::string argument; 
  for (int i = 1; i < argc; ++i) {
    argument = std::string(argv[i]);
    if (argument == "-help") print_help();
    else if (argument == "-bfc") bfc = false;
    else if (argument == "-wire") fill = false;
    else if (argument == "-zbuff") enable_z_buffer = false;
    else if (argument == "-f") {
      /*
      if (file) {
        std::cout << "File already specified and probably read too." << std::endl;
        ++i;
        continue;
      }
      */
      models.push_back(new Model(std::string(argv[i + 1])));
      file = true;
      ++i;
    }
    else if (argument == "-c") {
      std::string col_arg = std::string(argv[i + 1]);
      ++i;
      if (col_arg == "red")         col = RED;
      else if (col_arg == "blue")   col = BLUE;
      else if (col_arg == "orange") col = ORANGE;
      else if (col_arg == "yellow") col = YELLOW;
      else if (col_arg == "green")  col = GREEN;
      else if (col_arg == "white")  col = WHITE;
      else if (col_arg == "gray")   col = GRAY;
      else if (col_arg == "random") random_colour = true;
      else {
        std::cout << "Unknown colour. Defaulting to orange." << std::endl;
        col = ORANGE;
      }
    }
    else {
      std::cout << "Unknown argument: " << argument << "." << std::endl;
      exit(0);
    } 
  }

  if (!file) {
    std::cout << "No file specified. Use '-help' argument for instructions." << std::endl;
    exit(0);
  }
  colour = choose_colour(col);
  // Some initial transformations (view and perspective matrices)

  glm::mat4 view = glm::mat4(1.0f);
  viewpos = glm::vec3(0.0f, 0.0f, -10.0f);
  viewpos_n = glm::normalize(viewpos);
  view = glm::translate(view, viewpos);
  glm::mat4 perspective = glm::perspective(glm::radians(15.0f), 1.0f, 0.1f, 100.0f);
  persview = perspective * view;

  // Prepare the GTK-window and all
  gtk_init (NULL, NULL);
  GtkWidget *window, *box;
  image = GTK_IMAGE (gtk_image_new());
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(window), false);
  gtk_window_set_title(GTK_WINDOW(window), "Hahmontaja Manninen");
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET(image), TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_add_tick_callback(window, render, NULL, NULL);
  gtk_widget_show_all (window);
  gtk_main();
}
