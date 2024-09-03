#include <gtk/gtk.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define MAX_FLOAT std::numeric_limits<float>::max();
#define RAND_MAX 255;

const int width = 400;
const int height = 300;
const int halfw = 200;
const int halfh = 150;
const int scale = 70;

GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
GtkImage *image;
unsigned int pixel_amount = width * height;

std::vector<glm::vec3> vertices;
std::vector<glm::vec3> work_vertices;

std::vector<glm::vec3> normals;
std::vector<glm::vec3> work_normals;

std::vector<glm::ivec3> faces;
std::vector<glm::ivec3> colours;

float z_buff[width*height];

glm::mat4 persview;
glm::vec3 viewpos;
glm::vec3 viewpos_n;

enum Colour {
  RED,
  BLUE,
  ORANGE,
  YELLOW,
  GREEN,
  WHITE
};

// Parameters for rendering
bool bfc = false;
bool fill = false;
bool random_colour = false;
Colour col;

// A pixel drawing function.
inline void draw_pixel (int x, int y, char red, char green, char blue) {
  if (y < 0 || y > height - 1) return;
  if (x < 0 || x > width - 1) return;
  *(pixels + (height - y) * rowstride + x * 3) = red;
  *(pixels + (height - y) * rowstride + x * 3 + 1) = green;
  *(pixels + (height - y) * rowstride + x * 3 + 2) = blue;
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

// I actually don't know what this does lol.
inline int sign(int x1, int y1, int x2, int y2, int x3, int y3) {
  return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
}

// Check if the point is within the triangle
inline bool point_in_tri(int x1, int y1, int x2, int y2, int x3, int y3, int x, int y) {
  int d1, d2, d3;
  bool has_neg, has_pos;

  d1 = sign(x, y, x1, y1, x2, y2);
  d2 = sign(x, y, x2, y2, x3, y3);
  d3 = sign(x, y, x3, y3, x1, y1);

  has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
  has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

  return !(has_neg && has_pos);
}

inline void triangle_w(int p1x, int p1y, int p2x, int p2y, int p3x, int p3y, char r, char g, char b) {
  line(p1x, p1y, p2x, p2y, r, g, b);
  line(p2x, p2y, p3x, p3y, r, g, b);
  line(p3x, p3y, p1x, p1y, r, g, b);
}

// Draw a triangle
inline void triangle_f(
  int p1x, int p1y, int p2x, int p2y, int p3x, int p3y, 
  glm::vec3 normal, unsigned char r, unsigned char g, unsigned char b) {

  // Calculate normals here with cross products.
  // Implement back face culling, diffuse shading and
  // z-buffer, so that we'll get something meaningful

  float intensity = std::min(0.9f, std::max(0.0f, glm::dot(normal, -viewpos_n)));

  r = (unsigned char)(intensity * r);
  g = (unsigned char)(intensity * g);
  b = (unsigned char)(intensity * b);

  // Create the bounding box
  int bbox_min_x = width;
  int bbox_min_y = height;
  int bbox_max_x = 0;
  int bbox_max_y = 0;

  if (p1x < bbox_min_x) bbox_min_x = p1x;
  if (p1x > bbox_max_x) bbox_max_x = p1x;
  if (p1y < bbox_min_y) bbox_min_y = p1y;
  if (p1y > bbox_max_y) bbox_max_y = p1y;

  if (p2x < bbox_min_x) bbox_min_x = p2x;
  if (p2x > bbox_max_x) bbox_max_x = p2x;
  if (p2y < bbox_min_y) bbox_min_y = p2y;
  if (p2y > bbox_max_y) bbox_max_y = p2y;

  if (p3x < bbox_min_x) bbox_min_x = p3x;
  if (p3x > bbox_max_x) bbox_max_x = p3x;
  if (p3y < bbox_min_y) bbox_min_y = p3y;
  if (p3y > bbox_max_y) bbox_max_y = p3y;

/*
  // Draw bounding boxes, just for fun
  line(bbox_min_x, bbox_min_y, bbox_max_x, bbox_min_y, 255, 255, 0);
  line(bbox_max_x, bbox_min_y, bbox_max_x, bbox_max_y, 255, 255, 0);
  line(bbox_max_x, bbox_max_y, bbox_min_x, bbox_max_y, 255, 255, 0);
  line(bbox_min_x, bbox_max_y, bbox_min_x, bbox_min_y, 255, 255, 0);
*/

  // Check, if the pixels within the bounding box are in
  // the triangle. Probably has room for optimization.
  for (int x = bbox_min_x; x < bbox_max_x; ++x) {
    for (int y = bbox_min_y; y < bbox_max_y; ++y) {
      if (point_in_tri(p1x, p1y, p2x, p2y, p3x, p3y, x, y)) draw_pixel(x, y, r, g, b);
    }
  }
}

// Clear the screen (black)
inline void clear_screen() {
  for (unsigned int x = 0; x < pixel_amount; ++x) {
    *(pixels + x*3) = 0;
    *(pixels + x*3+1) = 0;
    *(pixels + x*3+2) = 0;
  }
}

inline void clear_z_buff() {
  float m = std::numeric_limits<float>::max();
  for (unsigned int x = 0; x < pixel_amount; ++x) {
    z_buff[x] = MAX_FLOAT;
  }
}

static gint64 old_time = 0;
static gint64 delta_time = 0;

// Auxiliary variables for vector
// calculations and drawing
glm::vec3 v;
glm::vec4 wv;
glm::ivec3 face;
glm::ivec3 colour = glm::ivec3(255, 127, 0);
glm::vec3 v1;
glm::vec3 v2;
glm::vec3 v3;
glm::vec3 n;
float wvw;

// Render loop
gboolean render (GtkWidget *widget, GdkFrameClock *clock, gpointer data) {

  // FPS
  gint64 time = gdk_frame_clock_get_frame_time(clock);
  delta_time = time - old_time;
  old_time = time;
  float delta_b = delta_time/1000000.0;
//  std::cout << (int)(1.0/delta_b) << std::endl;

  clear_screen();
//  clear_z_buff();

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::rotate(model, time/1000000.0f, glm::vec3(0, 1, 0));

  // Transform the vertices here using model and
  // perspective matrices and untransformed vertices
  int vlen = vertices.size();
  for(int i = 0; i < vlen; ++i) {
    v = vertices[i];
    wv.x = v.x;
    wv.y = v.y;
    wv.z = v.z;
    wv.w = 1.0;
    wv = persview * model * wv;
    wvw = wv.w;
    wv.x = wv.x/wvw;
    wv.y = wv.y/wvw;
    wv.z = wv.z/wvw;
    work_vertices[i].x = wv.x;
    work_vertices[i].y = wv.y;
    work_vertices[i].z = wv.z;
  }

  int nlen = normals.size();
  for (int i = 0; i < nlen; ++i) {
    work_normals[i] = glm::normalize(glm::mat3(model) * normals[i]);
  }

  // Draw model using faces and transformed vertices
  int flen = faces.size();
  for (int i = 0; i < flen; ++i) {
    face = faces[i];
    v1 = work_vertices[face.x-1];
    v2 = work_vertices[face.y-1];
    v3 = work_vertices[face.z-1];
    float dot = glm::dot(glm::normalize(glm::cross(v3-v1, v2-v1)), viewpos_n);
    // Back face culling
    if (dot < 0 && bfc) continue;
    if (fill) {
      if (random_colour) {
        colour = colours[i];
      }
      triangle_f(
        halfw + (int)(v1.x * scale), halfh + (int)(v1.y * scale),
        halfw + (int)(v2.x * scale), halfh + (int)(v2.y * scale),
        halfw + (int)(v3.x * scale), halfh + (int)(v3.y * scale),
        work_normals[i], colour.x, colour.y, colour.z
      );
    } else {
      triangle_w(
        halfw + (int)(v1.x * scale), halfh + (int)(v1.y * scale),
        halfw + (int)(v2.x * scale), halfh + (int)(v2.y * scale),
        halfw + (int)(v3.x * scale), halfh + (int)(v3.y * scale),
        250, 125, 0
      );
    }
  }
  gtk_image_set_from_pixbuf (image, pixbuf);
  return 1;
}

glm::vec3 nc1;
glm::vec3 nc2;

// Function for reading .obj files
void read(std::string filename) {

  std::ifstream file(filename);
  std::string buffer;

  float f1, f2, f3;
  int i1, i2, i3;
  std::string s;

  unsigned int vs = 0;
  unsigned int fs = 0;

  while(std::getline(file, buffer)) {
    std::stringstream ss(buffer);
    ss >> s;
    if (s == "v") {
      ss >> f1 >> f2 >> f3;
      vertices.push_back(glm::vec3(f1, f2, f3));
      ++vs;
    }
    else if (s == "f") {
      ss >> i1 >> i2 >> i3;
      faces.push_back(glm::ivec3(i1, i2, i3));
      ++fs;
    }
  }
  for (unsigned int i = 0; i < vs; ++i) {
    work_vertices.push_back(glm::vec3(0.0));
  }
  for (unsigned int i = 0; i < fs; ++i) {
    colours.push_back(glm::ivec3(std::rand(), std::rand(), std::rand()));
    nc1 = vertices[faces[i].z - 1] - vertices[faces[i].x - 1];
    nc2 = vertices[faces[i].y - 1] - vertices[faces[i].x - 1];
    normals.push_back(glm::cross(nc2, nc1));
    work_normals.push_back(glm::vec3(0.0));
  }
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
  }
}

void print_help() {
  std::cout << "Possible arguments are:" << std::endl;
  std::cout << " * '-help'                   > Displays this message. " << std::endl;
  std::cout << " * '-bfc'                    > Enalbles back face culling. " << std::endl;
  std::cout << " * '-fill'                   > Draws filled triangles (not wireframe)." << std::endl;
  std::cout << " * '-f [path/to/file.obj]'  > Loads the specified .obj file." << std::endl;
  std::cout << " * '-c [colour]'            > specifies the object colour." << std::endl;
  std::cout << " * Colours: 'red', 'green', 'blue', 'orange', 'yellow', 'white', 'random'." << std::endl << std::endl;
}

int main(int argc, const char* argv[]) {

  bool file = false;
  std::string argument; 
  for (int i = 1; i < argc; ++i) {
    argument = std::string(argv[i]);
    if (argument == "-help") print_help();
    else if (argument == "-bfc") bfc = true;
    else if (argument == "-fill") fill = true;
    else if (argument == "-f") {
      if (file) {
        std::cout << "File already specified and probably read too." << std::endl;
        ++i;
        continue;
      }
      read(std::string(argv[i + 1]));
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

  // Some initial transformations
  glm::mat4 view = glm::mat4(1.0f);
  viewpos = glm::vec3(0.0f, 0.0f, -3.0f);
  viewpos_n = glm::normalize(viewpos);
  view = glm::translate(view, viewpos);
  glm::mat4 perspective = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
  persview = perspective * view;

  // Prepare the GTK-window and all
  gtk_init (NULL, NULL);
  GtkWidget *window, *box;
  image = GTK_IMAGE (gtk_image_new());
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_resizable(GTK_WINDOW(window), false);
  gtk_window_set_title(GTK_WINDOW(window), "Raster");
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET(image), TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_add_tick_callback(window, render, NULL, NULL);
  gtk_widget_show_all (window);
  gtk_main();
}
