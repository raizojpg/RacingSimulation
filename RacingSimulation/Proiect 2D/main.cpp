#include <iostream>
#include <windows.h>

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>

#include <GL/glew.h>
#include <GL/freeglut.h>

#include "loadShaders.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "SOIL.h"

namespace App {

  using std::cout;
  using std::endl;

  struct Point { float x, y; };

  // Window dimensions
  static const GLfloat kWinW = 800.f;
  static const GLfloat kWinH = 600.f;

  // Coordinate system bounds
  static const float xMin = 0.f, xMax = 1600.f;
  static const float yMin = 0.f, yMax = 1200.f;

  // Car dimensions
  static const float HALF_W = 50.0f;
  static const float HALF_H = 25.0f;
  static const float SAFE_X = 2 * HALF_W + 15.0f;
  static const float SAFE_Y = 2 * HALF_H + 15.0f;

  static const float PI = 3.141592f;
  static const float BASE_SPEED = 500.0f;
  static const float TURN_SPEED = 5.0f;    
  static const float TURN_AHEAD = 35.0f;

  int gSwapBackground = 0;
  int gNrLaps = 0;
  bool gFinishLineCrossed = false;

  // Utility function to wrap angles between -PI and PI
  inline float wrapAngle(float a) {
    const float TWO_PI = 2.0f * PI;
    while (a > PI) a -= TWO_PI;
    while (a <= -PI) a += TWO_PI;
    return a;
  }

  // Utility function to rotate towards a target angle
  inline float rotateTowards(float current, float target, float maxDelta) {
    float diff = wrapAngle(target - current);
    if (std::fabs(diff) <= maxDelta) return target;
    return current + ((diff > 0.f) ? 1.f : -1.f) * maxDelta;
  }

  // Define track corners and trigger points
  static const Point A{ 200, 300 }, B{ 1400, 300 }, C{ 1400, 900 }, D{ 200, 900 };
  static const Point E{ 750, 400 }, F{ 950, 400 }, G{ 1150, 400 };
  static const Point H{ 1050, 800 }, I{ 850, 800 }, J{ 650, 800 };

  static float gMouseX = 0.f, gMouseY = 0.f;
  static bool gCarsMove = false;

  struct GlProgram {
    GLuint id = 0;
    GLint uMyMatrix = -1;
    GLint uUseTexture = -1;

    void create(const char* vsPath, const char* fsPath) {
      id = LoadShaders(vsPath, fsPath);
      glUseProgram(id);
      uMyMatrix = glGetUniformLocation(id, "myMatrix");
      uUseTexture = glGetUniformLocation(id, "useTexture");
    }
    void destroy() {
      if (id) glDeleteProgram(id);
      id = 0;
    }
  };

  struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;

    void destroy() {
      if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
      if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
      if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    }
  };

  static GLuint gTextures[50] = { 0 };

  static GlProgram gProg;
  static Mesh gGroundMesh;
  static Mesh gCarMesh;

  static glm::mat4 gProj; 
  static glm::mat4 gMatrix; 

  struct Car {
    float x, y;
    float dx, dy;
    float angle;
    float targetAngle; 
    int textureId;

    Car(float px, float py, float vdx, float vdy, float initAngle, int tex)
      : x(px), y(py), dx(vdx), dy(vdy),
      angle(initAngle), targetAngle(initAngle), textureId(tex) {}
  };

  struct PlayerCar {
    float x = 350.f;
    float y = 325.f;
    float dx = BASE_SPEED * 2.f;
    float dy = 0.f;
    float angle = PI;
    int currentTextureIndex = 4; 

    glm::mat4 lastRotation = glm::rotate(glm::mat4(1.f), angle, glm::vec3(0, 0, 1));
  };

  // Utility function to bind and use a texture
  inline void useTexture(int id) {
    glActiveTexture(GL_TEXTURE0 + id);
    glBindTexture(GL_TEXTURE_2D, gTextures[id]);
    glUniform1i(glGetUniformLocation(gProg.id, "myTexture"), id);
    glUniform1i(gProg.uUseTexture, 1);
  }

  // Utility function to set the transformation matrix
  inline void setMatrix(const glm::mat4& m) {
    glUniformMatrix4fv(gProg.uMyMatrix, 1, GL_FALSE, glm::value_ptr(m));
  }

  inline void loadTexture(const char* path, int id) {
    glGenTextures(1, &gTextures[id]);
    glBindTexture(GL_TEXTURE_2D, gTextures[id]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int w, h;
    unsigned char* image = SOIL_load_image(path, &w, &h, 0, SOIL_LOAD_RGBA);
    if (!image) {
      std::cerr << "Error: Failed to load texture " << path
        << " (" << SOIL_last_result() << ")\n";
      glBindTexture(GL_TEXTURE_2D, 0);
      return;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  inline void loadAllTextures() {
    loadTexture("car1.png", 0);
    loadTexture("car2.png", 1);
    loadTexture("car3.png", 2);
    loadTexture("car4.png", 3);
    loadTexture("car5.png", 4);
    loadTexture("car6.png", 5);
    loadTexture("car7.png", 6);
    loadTexture("car8.png", 7);
    loadTexture("car9.png", 8);
    loadTexture("car10.png", 9);

    loadTexture("track1.png", 11);
    loadTexture("track2.png", 12);

	loadTexture("0.png", 20);
    loadTexture("1.png", 21);
    loadTexture("2.png", 22);
    loadTexture("3.png", 23);
    loadTexture("4.png", 24);
    loadTexture("5.png", 25);
    loadTexture("6.png", 26);
    loadTexture("7.png", 27);
    loadTexture("8.png", 28);
    loadTexture("9.png", 29);
  }

  inline void createGroundMesh(Mesh& m) {
    const GLfloat Vertices[] = {
        //background coords
	    0, 0, 0.0f, 1.0f,              0.8f, 0.8f, 0.8f,	 0.0f, 0.0f,
	    1600, 0, 0.0f, 1.0f,           0.8f, 0.8f, 0.8f,	 1.0f, 0.0f,
        1600, 1200, 0.0f, 1.0f,        0.8f, 0.8f, 0.8f,	 1.0f, 1.0f,
        0, 1200, 0.0f, 1.0f,           0.8f, 0.8f, 0.8f,	 0.0f, 1.0f,
        //lap counter first digit
		29.0f, 20.0f, 0.0f, 1.0f,      0.8f, 0.8f, 0.8f,	 0.0f, 0.0f,
		125.0f, 20.0f, 0.0f, 1.0f,     0.8f, 0.8f, 0.8f,	 1.0f, 0.0f,
		125.0f, 105.0f, 0.0f, 1.0f,    0.8f, 0.8f, 0.8f,	 1.0f, 1.0f,
		29.0f, 105.0f, 0.0f, 1.0f,     0.8f, 0.8f, 0.8f,	 0.0f, 1.0f,
		//lap counter second digit
		125.0f, 20.0f, 0.0f, 1.0f,    0.8f, 0.8f, 0.8f,	     0.0f, 0.0f,
		217.0f, 20.0f, 0.0f, 1.0f,    0.8f, 0.8f, 0.8f,	     1.0f, 0.0f,
		217.0f, 105.0f, 0.0f, 1.0f,   0.8f, 0.8f, 0.8f,	     1.0f, 1.0f,
		125.0f, 105.0f, 0.0f, 1.0f,   0.8f, 0.8f, 0.8f,	     0.0f, 1.0f,
    };
    const GLuint Indices[] = {
        0,1,2,
		2,3,0,

        4,5,6,
        6,7,4,

        8,9,10,
        10,11,8,
    };

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices), Indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)(4 * sizeof(GLfloat)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)(7 * sizeof(GLfloat)));

    glBindVertexArray(0);
  }

  inline void createCarMesh(Mesh& m) {
    const GLfloat Vertices[] = {
      -50, -25, 0.f, 1.f,           0.3f,0.3f,0.3f,    0.f, 0.f,
       50, -25, 0.f, 1.f,           0.3f,0.3f,0.3f,    1.f, 0.f, 
       50, 25, 0.f, 1.f,            0.3f,0.3f,0.3f,    1.f, 1.f, 
      -50, 25, 0.f, 1.f,            0.3f,0.3f,0.3f,    0.f, 1.f,

      -50, -25, 0.f, 1.f,           0.5f,0.5f,0.5f,    0.f, 0.f,
       50, -25, 0.f, 1.f,           0.5f,0.5f,0.5f,    1.f, 0.f, 
       50, 25, 0.f, 1.f,            0.5f,0.5f,0.5f,    1.f, 1.f, 
      -50, 25, 0.f, 1.f,            0.5f,0.5f,0.5f,    0.f, 1.f 
    };
    const GLuint Indices[] = {
        0,1,2, 2,3,0, 
        4,5,6, 6,7,4 
    };

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Indices), Indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)(4 * sizeof(GLfloat)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)(7 * sizeof(GLfloat)));

    glBindVertexArray(0);
  }

  // Function to provide an early hint for turning the car towards the next corner
  inline void applyEarlyTurnHint(Car& c) {
      const float x = c.x, y = c.y; 
      // Check if the car is moving right and approaching the bottom-right corner (B)
      if (c.dx > 0 && y <= B.y + HALF_H + TURN_AHEAD &&
          x >= B.x - HALF_W - TURN_AHEAD) {
          c.targetAngle = -PI / 2.0f; 
      }
      // Check if the car is moving up and approaching the top-right corner (C)
      if (c.dy > 0 && x >= C.x - HALF_W - TURN_AHEAD &&
          y >= C.y - HALF_H - TURN_AHEAD) {
          c.targetAngle = 0.0f;
      }
      // Check if the car is moving left and approaching the top-left corner (D)
      if (c.dx < 0 && y >= D.y - HALF_H - TURN_AHEAD &&
          x <= D.x + HALF_W + TURN_AHEAD) {
          c.targetAngle = PI / 2.0f; 
      }
      // Check if the car is moving down and approaching the bottom-left corner (A)
      if (c.dy < 0 && x <= A.x + HALF_W + TURN_AHEAD &&
          y <= A.y + HALF_H + TURN_AHEAD) {
          c.targetAngle = PI;
      }
  }

  // Function to update the car's position and direction when it reaches a corner
  inline void updateCarAtCorner(Car& c) {
    const float x = c.x, y = c.y;
    // Check if the car is at the bottom-right corner (B)
    if (x >= B.x - HALF_W && y <= B.y + HALF_H) {
      c.targetAngle = -PI / 2.0f;
      c.x = B.x - HALF_W;
      c.dx = 0.0f; c.dy = BASE_SPEED;
    }
    // Check if the car is at the top-right corner (C)
    if (x >= C.x - HALF_W && y >= C.y - HALF_H) {
      c.targetAngle = 0.0f;
      c.y = C.y - HALF_H;
      c.dx = -BASE_SPEED; c.dy = 0.0f;
    }
    // Check if the car is at the top-left corner (D)
    if (x <= D.x + HALF_W && y >= D.y - HALF_H) {
      c.targetAngle = PI / 2.0f;
      c.x = D.x + HALF_W;
      c.dx = 0.0f; c.dy = -BASE_SPEED;
    }
    // Check if the car is at the bottom-left corner (A)
    if (x <= A.x + HALF_W && y <= A.y + HALF_H) {
      c.targetAngle = PI;
      c.y = A.y + HALF_H;
      c.dx = BASE_SPEED; c.dy = 0.0f;
    }
  }

  static std::vector<Car> gCars; 
  static PlayerCar gPlayer;

  void onMouseMove(int x, int y) {
    gMouseX = xMin + x * (xMax - xMin) / kWinW;
    gMouseY = yMax - y * (yMax - yMin) / kWinH;
    glutPostRedisplay();
  }

  void onMouseClick(int button, int state, int, int) {
    if (button == GLUT_RIGHT_BUTTON) {
      gCarsMove = (state == GLUT_DOWN);
      glutPostRedisplay();
    }
  }

  void onSpecialInput(int key, int, int) {
    switch (key) {
    case GLUT_KEY_RIGHT:
      gPlayer.currentTextureIndex++;
      if (gPlayer.currentTextureIndex > 9) gPlayer.currentTextureIndex = 0;
      break;
    case GLUT_KEY_LEFT:
      gPlayer.currentTextureIndex--;
      if (gPlayer.currentTextureIndex < 0) gPlayer.currentTextureIndex = 9;
      break;
    case 27: //ESC
      break;
    }
  }

  // Initializes OpenGL settings and resources
  void initGL() {
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gProg.create("Shader.vert", "Shader.frag");

    createGroundMesh(gGroundMesh);
    createCarMesh(gCarMesh);

    loadAllTextures();

    gProj = glm::ortho(xMin, xMax, yMin, yMax);

    // Register input event handlers
    glutPassiveMotionFunc(onMouseMove);
    glutMouseFunc(onMouseClick);
    glutSpecialFunc(onSpecialInput);
  }

  void destroyGL() {
    gProg.destroy();
    gGroundMesh.destroy();
    gCarMesh.destroy();
  }

  void initScene() {
    gCars.emplace_back(600.f, 325.f, BASE_SPEED, 0.f, PI, 0);
    gCars.emplace_back(1375.f, 450.f, 0.f, BASE_SPEED, -PI / 2.f, 1);
    gCars.emplace_back(950.f, 875.f, -BASE_SPEED, 0.f, 0.f, 2);
    gCars.emplace_back(225.f, 750.f, 0.f, -BASE_SPEED, PI / 2.f, 3);
  }

  void drawLapNumber(int lapNumber) {
      glBindVertexArray(gGroundMesh.vao);
	  int firstDigit = lapNumber / 10 % 10;
	  int secondDigit = lapNumber % 10;
	  useTexture(20 + firstDigit);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(GLuint)));
	  useTexture(20 + secondDigit);
	  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(12 * sizeof(GLuint)));
	  glBindVertexArray(0);
  }
  
  void drawGround() {
    glBindVertexArray(gGroundMesh.vao);

    gMatrix = gProj;
    setMatrix(gMatrix);
    if (gSwapBackground < 500)
        useTexture(11);
	else
        useTexture(12);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(0));

    drawLapNumber(gNrLaps);
    glBindVertexArray(0);
    gSwapBackground++;
    if(gSwapBackground == 1000)
		gSwapBackground = 0;
  }

  // Updates the positions and angles of cars, and renders them on the screen
  void updateAndDrawCars(float dt) {
    glBindVertexArray(gCarMesh.vao);

    for (Car& c : gCars) {
      if (gCarsMove) {
        c.x += c.dx * dt;
        c.y += c.dy * dt;

        applyEarlyTurnHint(c);
        updateCarAtCorner(c);
      }
      // Smoothly rotate the car towards its target angle
      c.angle = rotateTowards(c.angle, c.targetAngle, TURN_SPEED * dt);

      glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(c.x, c.y, 0.f));
      glm::mat4 R = glm::rotate(glm::mat4(1.f), c.angle, glm::vec3(0, 0, 1));
      gMatrix = gProj * T * R;
      setMatrix(gMatrix);
      useTexture(c.textureId);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(0));
    }

    glBindVertexArray(0);
  }

  void updatePlayerTiltForCorner(glm::mat4& rotMat, float setAngle) {
    gPlayer.angle = setAngle;
    rotMat = glm::rotate(glm::mat4(1.f), setAngle, glm::vec3(0, 0, 1));
  }

  void updatePlayer(float dt) {
    if (gCarsMove) {
      gPlayer.x += gPlayer.dx * dt;
      gPlayer.y += gPlayer.dy * dt;
    }

    glm::mat4 localRot = glm::rotate(glm::mat4(1.f), gPlayer.angle, glm::vec3(0, 0, 1));

    float& x = gPlayer.x;
    float& y = gPlayer.y;

    if (gCarsMove) {
       // Corner B: Adjust position and direction when reaching this corner
      if (x >= B.x - HALF_W && y <= 400.0f - HALF_H) {
        localRot = glm::rotate(glm::mat4(1.f), App::PI / 4.f, glm::vec3(0, 0, 1));
        gPlayer.angle = -PI / 2.f;
        x = B.x - HALF_W; gPlayer.dx = 0.f; gPlayer.dy = BASE_SPEED * 2.f;
      }
      // Corner C: Adjust position and direction when reaching this corner
      if (x >= C.x - HALF_W && y >= 800.0f + HALF_H) {
        localRot = glm::rotate(glm::mat4(1.f), -App::PI / 4.f, glm::vec3(0, 0, 1));
        gPlayer.angle = 0.f;
        y = C.y - HALF_H; gPlayer.dx = -BASE_SPEED * 2.f; gPlayer.dy = 0.f;
      }
      // Corner D: Adjust position and direction when reaching this corner
      if (x <= D.x + HALF_W && y >= 800.0f + HALF_H) {
        localRot = glm::rotate(glm::mat4(1.f), App::PI / 4.f, glm::vec3(0, 0, 1));
        gPlayer.angle = PI / 2.f;
        x = D.x + HALF_W; gPlayer.dx = 0.f; gPlayer.dy = -BASE_SPEED * 2.f;
      }
      // Corner A: Adjust position and direction when reaching this corner
      if (x <= A.x + HALF_W && y <= 400.0f - HALF_H) {
        localRot = glm::rotate(glm::mat4(1.f), -App::PI / 4.f, glm::vec3(0, 0, 1));
        gPlayer.angle = PI;
        y = A.y + HALF_H; gPlayer.dx = BASE_SPEED * 2.f; gPlayer.dy = 0.f;
      }
    }

    // Handle collisions with other cars to maintain safe distances
    if (gCarsMove) {
      for (const Car& c : gCars) {
         // Horizontal collision handling
        if (gPlayer.dy == 0.f && std::fabs(y - c.y) <= HALF_H + 1.0f) {
          if (gPlayer.dx > 0.f && c.x > x && c.x - x < SAFE_X) {
            if (x > 600 && x < 1000 && y <= 400.0f - HALF_H) y = 375.0f;
            else x = c.x - SAFE_X;
          }
          if (gPlayer.dx < 0.f && c.x < x && x - c.x < SAFE_X) {
            if (x > 600 && x < 1000 && y >= 800.0f + HALF_H) y = 825.0f;
            else x = c.x + SAFE_X;
          }
        }
        // Vertical collision handling
        if (gPlayer.dx == 0.f && std::fabs(x - c.x) <= HALF_W + 1.0f) {
          if (gPlayer.dy > 0.f && c.y > y && c.y - y < SAFE_Y) y = c.y - SAFE_Y;
          if (gPlayer.dy < 0.f && c.y < y && y - c.y < SAFE_Y) y = c.y + SAFE_Y;
        }
      }
    }

    // Adjust the player's tilt based on specific track sections
    if (gCarsMove) {
      if (x > 600 && x < 800 && y < E.y) {
        localRot = glm::rotate(glm::mat4(1.f), PI + PI / 6.f, glm::vec3(0, 0, 1));
      }
      if (x > 800 && x < 1000 && y > H.y) {
        localRot = glm::rotate(glm::mat4(1.f), PI / 6.f, glm::vec3(0, 0, 1));
      }

      if (x > 800 && x < 1000 && y < F.y) y = 375.f;
      if (x > 600 && x < 800 && y > I.y) y = 825.f;

      if (x > 1000 && x < 1200 && y < G.y) {
        localRot = glm::rotate(glm::mat4(1.f), PI - PI / 6.f, glm::vec3(0, 0, 1));
      }
      if (x > 400 && x < 600 && y > J.y) {
        localRot = glm::rotate(glm::mat4(1.f), -PI / 6.f, glm::vec3(0, 0, 1));
      }

      if (x > 1000 && x < 1200 && y < F.y) y = 325.f;
      if (x > 400 && x < 600 && y > I.y) y = 875.f;
    }

    gPlayer.lastRotation = localRot;
  }

  void drawPlayer() {
    glBindVertexArray(gCarMesh.vao);
    // Check if the player is crossing the finish line
    if (gPlayer.x >= 790 && gPlayer.x <= 810) {
        if (gPlayer.y <= 500) {
			gFinishLineCrossed = false;
        }
        else {
            if (gFinishLineCrossed == false)
                gNrLaps++;
			gFinishLineCrossed = true;
        }
    }
    glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(gPlayer.x, gPlayer.y, 0.f));
    gMatrix = gProj * T * gPlayer.lastRotation;
    setMatrix(gMatrix);

    useTexture(gPlayer.currentTextureIndex);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(6 * sizeof(GLuint)));

    glBindVertexArray(0);
  }

  void render() {
    // Calculate the time delta (dt) since the last frame
    static int prevMs = glutGet(GLUT_ELAPSED_TIME);
    int currMs = glutGet(GLUT_ELAPSED_TIME);
    float dt = (currMs - prevMs) / 1200.0f;
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.05f) dt = 0.05f;
    prevMs = currMs;

    glClear(GL_COLOR_BUFFER_BIT);

    drawGround();

    updateAndDrawCars(dt);

    updatePlayer(dt);
    drawPlayer();

    glutSwapBuffers();
    glFlush();
  }

  void onIdle() {
    render();
  }

} 

int main(int argc, char* argv[]) {
  using namespace App;

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize((int)kWinW, (int)kWinH);
  glutInitWindowPosition(100, 100);
  glutCreateWindow("RacingSimulation 2D");

  glewInit();

  initGL();
  initScene();

  glutDisplayFunc(render);
  glutIdleFunc(onIdle);
  glutCloseFunc(destroyGL);

  glutMainLoop();
  return 0;
}