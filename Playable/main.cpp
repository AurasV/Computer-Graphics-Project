// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <memory>           // For std::unique_ptr
#include <algorithm>        // For std::remove_if
#include <random>           // For random number generation
#include <string>           // For texture paths

#define _USE_MATH_DEFINES   // For PI
#include <cmath>            // For fabs in particle velocity

// Dependencies
#include "dependente/glew/glew.h"
#include "dependente/glfw/glfw3.h"
#include "dependente/glm/glm.hpp"
#include "dependente/glm/gtc/matrix_transform.hpp"
#include "dependente/glm/gtc/type_ptr.hpp"

// Include helpers
#include "Camera/camera.h"
#include "shader.hpp"

// Single-file header for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// For Audio
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h" 


// Define element types
enum ElementType {
    EARTH = 0,
    WATER,
    FIRE,
    AIR,
    NUM_ELEMENT_TYPES
};

// Define game states
enum class GameState {
    RUNNING,
    GAME_OVER_WIN,
    GAME_OVER_LOSE
};


class Game;

// This function handles loading image data into an OpenGL texture.
static GLuint loadTextureUtility(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID); // Generate a new OpenGL texture ID
    glBindTexture(GL_TEXTURE_2D, textureID); // Bind it as a 2D texture

    // Set texture wrapping parameters (how texture coordinates outside 0-1 range behave)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Clamp to edge horizontally
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Clamp to edge vertically

    // Set texture filtering parameters (how texture scales up/down)
    // GL_LINEAR_MIPMAP_LINEAR for minification (when texture is smaller than pixels)
    // GL_LINEAR for magnification (when texture is larger than pixels)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // Flip image vertically (OpenGL expects textures to start from bottom-left)
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0); // Load image data using stb_image

    if (data) {
        // Determine the OpenGL internal format based on number of channels
        GLenum format = GL_RGB;
        if (nrChannels == 4) format = GL_RGBA; // Has alpha channel
        else if (nrChannels == 1) format = GL_RED; // Grayscale

        // Upload texture data to the GPU
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps for smoother scaling
        std::cout << "Successfully loaded texture: " << path << " (Width: " << width << ", Height: " << height << ", Channels: " << nrChannels << ")" << std::endl;
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        textureID = 0; // Set textureID to 0 to indicate failure
    }
    stbi_image_free(data); // Free image data from CPU memory
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture
    return textureID; // Return the OpenGL texture ID
}


// Base class for game objects
class GameObject {
protected:
    GLuint VAO, VBO;                // Vertex Array Object, Vertex Buffer Object
    std::vector<float> vertices;    // Stores vertex data (position, normal, texture coordinates)
    glm::vec3 position;             // World position of the object
    glm::vec3 scale;                // Scale of the object (width, height, depth)
    glm::vec4 color;                // Base color or tint for the object
public:                             // Made public so Game can set it for score digits
    GLuint textureID;               // OpenGL texture ID for the object's texture
protected:

    void setupMesh();                       // Initializes VAO and VBO with vertex data
    void loadTexture(const char* path);     // Loads texture using the utility function

public:
    GameObject();
    virtual ~GameObject();

    // Virtual functions for object-specific behavior
    virtual void init();
    // Modified update to accept a Game* for potential interaction
    virtual void update(float deltaTime, Game* gameInstance = nullptr);
    virtual void draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection);

    // Getters and Setters
    glm::vec3 getPosition() const { return position; }
    void setPosition(const glm::vec3& pos) { position = pos; }

    glm::vec3 getScale() const { return scale; }
    void setScale(const glm::vec3& s) { scale = s; }

    glm::vec4 getColor() const { return color; }
    void setColor(const glm::vec4& c) { this->color = c; } 
};

// Constructor: Initializes OpenGL IDs to 0, sets default position, scale, and color.
GameObject::GameObject() : VAO(0), VBO(0), textureID(0), position(0.0f), scale(1.0f), color(1.0f) {}

// Destructor: Cleans up OpenGL resources (buffers, arrays, arrays).
GameObject::~GameObject() {
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
    }
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
    }
    // Note: textureID cleanup should be handled by the owning class if it's shared/managed,
    // or if a specific texture belongs only to this GameObject.
    // For now, assume a texture is unique to an object if loaded via loadTexture().
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void GameObject::init() {
    // Default quad vertices for any 2D GameObject.
    // Format: Position (x,y,z), Normal (x,y,z), TexCoord (s,t)
    vertices = {
        // positions       // normals     // texture coords
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
         0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, // bottom-right
         0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // top-right

         0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // top-right
        -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, // top-left
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f  // bottom-left
    };
    setupMesh(); // Call setupMesh to initialize VAO/VBO with these vertices
}

// Sets up the VAO and VBO for the object's mesh.
void GameObject::setupMesh() {
    // If VAO/VBO already exist (e.g. for scoreDigitQuad which is reused), delete them first
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);

    glGenVertexArrays(1, &VAO); // Generate VAO
    glGenBuffers(1, &VBO);      // Generate VBO

    glBindVertexArray(VAO);             // Bind VAO
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // Bind VBO
    // Upload vertex data to VBO
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    // Define vertex attribute pointers for the shader
    // Layout 0: Position (3 floats)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Layout 1: Normal (3 floats) - even for 2D, included in vertex data
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Layout 2: Texture Coordinate (2 floats)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // Unbind VAO
}

// Loads a texture for the object using the global utility function.
void GameObject::loadTexture(const char* path) {
    textureID = loadTextureUtility(path);
}

// Draws the game object.
void GameObject::draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
    if (VAO == 0) { 
        std::cerr << "Attempted to draw GameObject with uninitialized VAO!" << std::endl;
        return;
    }

    glUseProgram(shaderProgram); // Use the specified shader program

    // Pass model, view, and projection matrices to the shader
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);    // Apply translation
    modelMatrix = glm::scale(modelMatrix, scale);           // Apply scaling
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    // Pass object color and texture usage flag to the shader
    unsigned int objColorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform4fv(objColorLoc, 1, glm::value_ptr(color));

    unsigned int useTextureLoc = glGetUniformLocation(shaderProgram, "useTexture");
    if (textureID != 0) {                           // If a texture is loaded
        glUniform1i(useTextureLoc, 1);              // Tell shader to use texture
        glActiveTexture(GL_TEXTURE0);               // Activate texture unit 0
        glBindTexture(GL_TEXTURE_2D, textureID);    // Bind the object's texture
        // Explicitly tell the shader's 'textureSampler' uniform to use texture unit 0
        glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0);
    }
    else {
        glUniform1i(useTextureLoc, 0);              // Tell shader not to use texture
    }

    glBindVertexArray(VAO);                             // Bind the object's VAO
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 8); // Draw the triangles
    glBindVertexArray(0);                               // Unbind VAO

    // Unbind texture after drawing to avoid unintended state changes
    if (textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}


// Basket class (inherits from GameObject)
class Basket : public GameObject {
private:
    ElementType currentType; // Current element type of the basket
    float speed;             // Movement speed of the basket
    float width, height;     // Dimensions for collision detection

public:
    Basket(float x, float y, float w, float h, float s);

    void init() override;
    void update(float deltaTime, Game* gameInstance = nullptr) override; // Keep signature consistent
    void draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) override;

    void moveLeft(float deltaTime);     // Move Left
    void moveRight(float deltaTime);    // Move Right
    void changeType(int direction);     // Cycles through element types

    ElementType getType() const { return currentType; }
    void setType(ElementType type) { currentType = type; }

    // Bounding box getters for collision detection
    float getLeft() const { return position.x - width / 2.0f; }
    float getRight() const { return position.x + width / 2.0f; }
    float getBottom() const { return position.y - height / 2.0f; }
    float getTop() const { return position.y + height / 2.0f; }
};

// Basket constructor: Sets up initial position, scale, speed, and default type.
Basket::Basket(float x, float y, float w, float h, float s)
    : width(w), height(h), speed(s) {
    position = glm::vec3(x, y, 0.0f);
    scale = glm::vec3(w, h, 1.0f);      // Scale quad to actual width and height
    currentType = EARTH;                // Default type

}

// Initializes the basket's mesh and loads its texture.
void Basket::init() {
    GameObject::init();                 // Call base class init to setup VAO/VBO
    loadTexture("textures/basket.png"); // Load the basket texture
}

// Draws the basket, setting its color based on its current element type.
void Basket::draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
    // Set the color based on the currentType to tint the basket texture
    switch (currentType) {
    case EARTH: setColor(glm::vec4(0.6f, 0.4f, 0.2f, 1.0f)); break; // Brown for Earth
    case WATER: setColor(glm::vec4(0.2f, 0.4f, 0.8f, 1.0f)); break; // Blue for Water
    case FIRE:  setColor(glm::vec4(0.8f, 0.2f, 0.2f, 1.0f)); break; // Red for Fire
    case AIR:   setColor(glm::vec4(0.7f, 0.9f, 1.0f, 1.0f)); break; // Light Blue/Cyan for Air
    default:    setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); break; // Default white (no tint)
    }

    GameObject::draw(shaderProgram, view, projection); // Call base class draw method
}

// Moves the basket left.
void Basket::moveLeft(float deltaTime) {
    position.x -= speed * deltaTime;
}

// Moves the basket right.
void Basket::moveRight(float deltaTime) {
    position.x += speed * deltaTime;
}

// Changes the basket's element type, cycling through available types.
void Basket::changeType(int direction) {
    currentType = static_cast<ElementType>((static_cast<int>(currentType) + direction + NUM_ELEMENT_TYPES) % NUM_ELEMENT_TYPES);
    std::cout << "Basket type changed to: " << currentType << std::endl; // Debug
}


// Orb class (inherits from GameObject)
class Orb : public GameObject {
private:
    ElementType type;               // Element type of the orb
    float fallSpeed;                // Speed at which the orb falls
    float width, height;            // Dimensions for collision detection

    float m_zigzagAmplitude;        // How far left/right the orb moves
    float m_zigzagFrequency;        // How fast the orb wiggles left/right
    float m_initialX;               // The starting X position for the zig-zag calculation
    float m_zigzagPhaseOffset;      // To make each orb's zig-zag unique

    float m_particleEmitTimer;      // Timer for particle emission
    float m_particleEmitInterval;   // How often to emit particles

public:
    Orb(float x, float y, float w, float h, ElementType t, float speed);

    void init() override;
    void update(float deltaTime, Game* gameInstance) override;
    void draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) override;

    ElementType getType() const { return type; }
    // Checks if the orb is off-screen (below the given Y coordinate).
    bool isOffScreen(float screenBottomY) const { return position.y + height / 2.0f < screenBottomY; }

    // Bounding box getters for collision detection
    float getLeft() const { return position.x - width / 2.0f; }
    float getRight() const { return position.x + width / 2.0f; }
    float getBottom() const { return position.y - height / 2.0f; }
    float getTop() const { return position.y + height / 2.0f; }
};

// Orb constructor: Sets up initial position, scale, type, and fall speed.
Orb::Orb(float x, float y, float w, float h, ElementType t, float speed)
    : type(t), fallSpeed(speed), width(w), height(h),
    m_particleEmitTimer(0.0f), m_particleEmitInterval(0.05f)
{
    position = glm::vec3(x, y, 0.0f);
    scale = glm::vec3(w, h, 1.0f);

    m_initialX = x; // Store the original spawn X position
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> ampDist(20.0f, 60.0f);        // Amplitude units
    std::uniform_real_distribution<float> freqDist(0.8f, 5.0f);         // Frequency oscillations per second
    std::uniform_real_distribution<float> phaseDist(0.0f, 4.0f * M_PI); // Random phase offset for variety

    m_zigzagAmplitude = ampDist(gen);
    m_zigzagFrequency = freqDist(gen);
    m_zigzagPhaseOffset = phaseDist(gen);
}

// Initializes the orb's mesh and loads its specific element texture.
void Orb::init() {
    const int numSegments = 30; // Number of segments to approximate the circle
    const float radius = 0.5f;  // Orb's quad is -0.5 to 0.5, so radius is 0.5 to fit in the scale.

    // Clear existing vertices
    vertices.clear();

    // Center vertex (for a fan-like triangulation)
    // Position (x,y,z), Normal (x,y,z), TexCoord (s,t)
    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);   // Center position
    vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);   // Normal (facing camera)
    vertices.push_back(0.5f); vertices.push_back(0.5f);                             // Center of texture

    for (int i = 0; i <= numSegments; ++i) {
        float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(numSegments);
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Position
        vertices.push_back(x); vertices.push_back(y); vertices.push_back(0.0f);
        // Normal (facing camera)
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
        // Texture Coordinates (map to a circle on the texture)
        vertices.push_back((x / (2.0f * radius)) + 0.5f); // Normalize x to 0-1 range and shift
        vertices.push_back((y / (2.0f * radius)) + 0.5f); // Normalize y to 0-1 range and shift
    }

    setupMesh(); 

    // Generate vertices for triangles
    vertices.clear();
    for (int i = 0; i < numSegments; ++i) {
        float angle1 = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(numSegments);
        float angle2 = 2.0f * M_PI * static_cast<float>(i + 1) / static_cast<float>(numSegments);

        // Triangle 1: Center vertex
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(0.0f);   // Pos
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);   // Normal
        vertices.push_back(0.5f); vertices.push_back(0.5f);                             // TexCoord (center)

        // Triangle 2: First point on circumference
        float x1 = radius * cos(angle1);
        float y1 = radius * sin(angle1);
        vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(0.0f);                               // Pos
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);                           // Normal
        vertices.push_back((x1 / (2.0f * radius)) + 0.5f); vertices.push_back((y1 / (2.0f * radius)) + 0.5f);   // TexCoord

        // Triangle 3: Second point on circumference
        float x2 = radius * cos(angle2);
        float y2 = radius * sin(angle2);
        vertices.push_back(x2); vertices.push_back(y2); vertices.push_back(0.0f);                               // Pos
        vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);                           // Normal
        vertices.push_back((x2 / (2.0f * radius)) + 0.5f); vertices.push_back((y2 / (2.0f * radius)) + 0.5f);   // TexCoord
    }

    setupMesh(); // Call base class setupMesh to initialize VAO/VBO with these vertices

    std::string texturePath;
    switch (type) {
    case EARTH: texturePath = "textures/earth_orb.png"; break;
    case WATER: texturePath = "textures/water_orb.png"; break;
    case FIRE:  texturePath = "textures/fire_orb.png";  break;
    case AIR:   texturePath = "textures/air_orb.png";   break;
    default:    texturePath = "textures/default_orb.png"; break; // Fallback texture (doesn't exist, shhh don't tell anyone)
    }
    loadTexture(texturePath.c_str());
}

// Draws the orb, setting its color (usually white to show full texture color).
void Orb::draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
    // Orbs primarily use their texture, so set color to white for no tinting.
    setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    GameObject::draw(shaderProgram, view, projection); // Call base class draw method
}



// Structure representing a single particle
struct Particle {
    glm::vec3 position;  // Current world position
    glm::vec3 velocity;  // Current velocity
    glm::vec4 color;     // Current color (includes alpha for transparency)
    float life;          // Current elapsed lifetime (0.0 to duration)
    float duration;      // Total duration of the particle's life
    float size;          // Initial size of the particle
    bool active;         // Flag indicating if the particle is currently active

    Particle() : position(0.0f), velocity(0.0f), color(1.0f), life(0.0f), duration(0.0f), size(1.0f), active(false) {}
};

// Class managing a pool of particles for a specific effect/texture
class ParticleSystem {
private:
    std::vector<Particle> particles;                        // Pool of particles
    unsigned int lastUsedParticle;                          // Index of the last used particle (for optimization)
    int maxParticles;                                       // Maximum number of particles in the pool
    GLuint particleVAO, particleVBO, particleInstanceVBO;   // OpenGL IDs for rendering
    std::string particleTexturePath;                        // Path to the particle's texture
    GLuint textureID;                                       // OpenGL texture ID for particles

    // Vertices for a single 2D quad that will be instanced for each particle
    std::vector<float> quadVertices = {
        // positions    // texCoords
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, // top-left
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f, // top-right
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // bottom-right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f  // bottom-left
    };
    std::vector<unsigned int> quadIndices = {
        0, 1, 2,    // First triangle
        2, 3, 0     // Second triangle
    };
    GLuint quadEBO; // Element Buffer Object for the quad indices

    unsigned int findUnusedParticle(); // Finds an inactive particle to reuse

public:
    ParticleSystem(int maxParticles, const std::string& texturePath = "");
    ~ParticleSystem();

    void init(); // Initializes OpenGL resources for the particle system
    void update(float deltaTime, const glm::vec3& cameraPos); // Updates all active particles
    void draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection); // Draws all active particles
    void emit(const glm::vec3& position, int count, ElementType type); // Emits new particles at a given position
};

// ParticleSystem constructor: Resizes the particle pool and stores texture path.
ParticleSystem::ParticleSystem(int maxParticles, const std::string& texturePath)
    : maxParticles(maxParticles), lastUsedParticle(0), particleTexturePath(texturePath), textureID(0) {
    particles.resize(maxParticles);
}

// ParticleSystem destructor: Cleans up OpenGL resources.
ParticleSystem::~ParticleSystem() {
    if (particleInstanceVBO != 0) glDeleteBuffers(1, &particleInstanceVBO);
    if (quadEBO != 0) glDeleteBuffers(1, &quadEBO);
    if (particleVBO != 0) glDeleteBuffers(1, &particleVBO);
    if (particleVAO != 0) glDeleteVertexArrays(1, &particleVAO);
    if (textureID != 0) glDeleteTextures(1, &textureID);
}

// Initializes OpenGL buffers and attributes for instanced particle rendering.
void ParticleSystem::init() {
    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glGenBuffers(1, &quadEBO);
    glGenBuffers(1, &particleInstanceVBO); // VBO for per-instance data

    glBindVertexArray(particleVAO);

    // Bind and fill VBO for the base quad vertices
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, quadVertices.size() * sizeof(float), &quadVertices[0], GL_STATIC_DRAW);

    // Bind and fill EBO for the quad indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, quadIndices.size() * sizeof(unsigned int), &quadIndices[0], GL_STATIC_DRAW);

    // Configure vertex attributes for the base quad (per-vertex data)
    glEnableVertexAttribArray(0); // Layout 0: aPos (position of quad vertex)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); // Layout 1: aTexCoord (texture coordinate of quad vertex)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Configure instance attributes (per-instance data)
    glBindBuffer(GL_ARRAY_BUFFER, particleInstanceVBO);
    // Allocate buffer for instance data: position (3), normalized life (1), color (4), size (1) = 9 floats per particle
    glBufferData(GL_ARRAY_BUFFER, maxParticles * (3 + 1 + 4 + 1) * sizeof(float), NULL, GL_STREAM_DRAW); // GL_STREAM_DRAW for frequent updates

    // Layout 2: instancePosition (position of the particle in world space)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced attribute (advance every instance)

    // Layout 3: instanceLife (normalized life 0.0 to 1.0)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // Layout 4: instanceColor (particle's color)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    // Layout 5: instanceSize (particle's initial size)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0); // Unbind VAO

    if (!particleTexturePath.empty()) {
        textureID = loadTextureUtility(particleTexturePath.c_str()); // Load particle texture
    }
}

// Finds an inactive particle in the pool to reuse.
unsigned int ParticleSystem::findUnusedParticle() {
    // Start search from last used particle for better distribution
    for (unsigned int i = lastUsedParticle; i < maxParticles; ++i) {
        if (!particles[i].active) {
            lastUsedParticle = i;
            return i;
        }
    }
    // If no unused particle found from last used to end, search from beginning
    for (unsigned int i = 0; i < lastUsedParticle; ++i) {
        if (!particles[i].active) {
            lastUsedParticle = i;
            return i;
        }
    }
    return -1; // All particles are currently active (consider increasing maxParticles)
}

// Emits a specified number of particles at a given position with type-specific properties.
void ParticleSystem::emit(const glm::vec3& position, int count, ElementType type) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> velDist(-1.0f, 1.0f); // For random velocity components
    std::uniform_real_distribution<float> lifeDist(0.5f, 1.5f);  // For random particle lifetime
    std::uniform_real_distribution<float> sizeDist(15.0f, 25.0f); // For random particle size

    for (int i = 0; i < count; ++i) {
        unsigned int particleIdx = findUnusedParticle();
        if (particleIdx != (unsigned int)-1) { // If an unused particle is found
            Particle& p = particles[particleIdx];
            p.active = true;
            p.position = position; // Start at the emission point
            p.duration = lifeDist(gen); // Random duration
            p.life = 0.0f; // Reset current life
            p.size = sizeDist(gen); // Random initial size

            // Set specific colors and initial velocities based on element type
            switch (type) {
            case EARTH:
                p.color = glm::vec4(0.4f, 0.2f, 0.0f, 1.0f); // Dark brown
                p.velocity = glm::vec3(velDist(gen) * 30.0f, velDist(gen) * 30.0f - 20.0f, 0.0f); // Slightly downward bias
                break;
            case WATER:
                p.color = glm::vec4(0.5f, 0.7f, 1.0f, 1.0f); // Light blue
                p.velocity = glm::vec3(velDist(gen) * 40.0f, velDist(gen) * 40.0f + 10.0f, 0.0f); // Slightly upward bias
                break;
            case FIRE:
                p.color = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange/Yellow
                p.velocity = glm::vec3(velDist(gen) * 60.0f, fabs(velDist(gen)) * 80.0f + 20.0f, 0.0f); // Strong upward bias
                break;
            case AIR:
                p.color = glm::vec4(0.8f, 0.9f, 1.0f, 1.0f); // Very light blue/white
                p.velocity = glm::vec3(velDist(gen) * 70.0f, velDist(gen) * 70.0f, 0.0f); // More spread out, higher speed
                p.size = sizeDist(gen) * 1.2f; // Slightly larger for air
                break;
            default:
                p.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
                break;
            }
        }
    }
}

// Updates the state of all active particles.
void ParticleSystem::update(float deltaTime, const glm::vec3& cameraPos) {
    for (auto& p : particles) {
        if (p.active) {
            p.life += deltaTime; // Advance particle's life
            if (p.life > p.duration) {
                p.active = false; // Deactivate if lifetime exceeded
            }
            else {
                p.position += p.velocity * deltaTime; // Update position based on velocity
                p.velocity *= (1.0f - 0.5f * deltaTime); // Apply simple friction/drag (adjust 0.5f for effect)
            }
        }
    }
}

// Draws all active particles using instanced rendering.
void ParticleSystem::draw(GLuint shaderProgram, const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(shaderProgram); // Use the particle shader

    // Prepare instance data for active particles
    std::vector<float> instanceData;
    int numActiveParticles = 0;
    for (const auto& p : particles) {
        if (p.active) {
            numActiveParticles++;
            instanceData.push_back(p.position.x);
            instanceData.push_back(p.position.y);
            instanceData.push_back(p.position.z);
            instanceData.push_back(p.life / p.duration); // Normalized life (0.0 to 1.0) for shader
            instanceData.push_back(p.color.r);
            instanceData.push_back(p.color.g);
            instanceData.push_back(p.color.b);
            instanceData.push_back(p.color.a);
            instanceData.push_back(p.size);
        }
    }

    if (numActiveParticles == 0) return; // No particles to draw

    // Update the instance VBO with the new data
    glBindBuffer(GL_ARRAY_BUFFER, particleInstanceVBO);
    // Use glBufferSubData to update only a portion of the buffer if needed,
    // or re-allocate if size changes significantly. For simplicity, we update the whole buffer.
    glBufferData(GL_ARRAY_BUFFER, maxParticles * (3 + 1 + 4 + 1) * sizeof(float), NULL, GL_STREAM_DRAW); // GL_STREAM_DRAW for frequent updates
    glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind

    // Pass view and projection matrices to the particle shader
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    // Pass current time for shader animations (if any)
    // glUniform1f(glGetUniformLocation(shaderProgram, "currentTime"), glfwGetTime()); // If you need a global time uniform

    // Bind and use particle texture if available
    if (textureID != 0) {
        glUniform1i(glGetUniformLocation(shaderProgram, "useParticleTexture"), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "particleTexture"), 0);
    }
    else {
        glUniform1i(glGetUniformLocation(shaderProgram, "useParticleTexture"), 0);
    }

    glBindVertexArray(particleVAO); // Bind the particle system's VAO
    // Draw instances: draw the quad `numActiveParticles` times
    glDrawElementsInstanced(GL_TRIANGLES, quadIndices.size(), GL_UNSIGNED_INT, 0, numActiveParticles);
    glBindVertexArray(0); // Unbind VAO

    // Unbind texture after drawing
    if (textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glUseProgram(0); // Unbind shader program
}


// Game class: Manages game state, objects, and logic.
class Game {
private:
    int screenWidth, screenHeight; // Current dimensions of the game window
    int score;                     // Player's score
    std::unique_ptr<Basket> playerBasket; // The player's basket
    std::vector<std::unique_ptr<Orb>> fallingOrbs; // List of active orbs
    std::vector<std::unique_ptr<ParticleSystem>> particleSystems; // One particle system for each element type

    float orbSpawnTimer;      // Timer for spawning new orbs
    float orbSpawnInterval;   // How often new orbs spawn
    float orbFallSpeed;       // Speed at which orbs fall
    float basketBottomMargin; // Distance of the basket from the bottom edge

    std::mt19937 rng; // Random number generator engine

    // For Score Display
    std::vector<GLuint> m_digitTextures; // Stores texture IDs for digits 0-9
    GLuint m_minusTexture;              // Texture for the minus sign (NEW)
    GameObject m_scoreDigitQuad;        // A reusable quad for drawing each digit
    float m_digitWidth = 20.0f;         // Visual width of a single digit on screen
    float m_digitHeight = 30.0f;        // Visual height of a single digit on screen
    float m_scoreDisplayMarginX = 20.0f; // Margin from the right edge
    float m_scoreDisplayMarginY = 20.0f; // Margin from the top edge
    glm::vec4 m_lastDestroyedOrbColor;   // Stores the color of the last orb destroyed

    GameState m_currentState; // Current state of the game
    GameObject m_messageQuad; // Reusable quad for displaying messages (Game Over, You Win, Restart)
    GLuint m_gameOverTextureID;
    GLuint m_youWinTextureID;
    GLuint m_pressRToRestartTextureID;
    float m_messageWidth = 500.0f; // Width for game over / win messages
    float m_messageHeight = 120.0f; // Height for game over / win messages
    float m_restartMessageWidth = 300.0f; // Width for restart prompt
    float m_restartMessageHeight = 50.0f; // Height for restart prompt

    ma_engine m_audioEngine;
    ma_sound m_correctCatchSound;
    ma_sound m_wrongCatchSound;


    void spawnOrb();       // Creates and adds a new orb
    void checkCollisions(); // Checks for collisions between orbs and basket
    // Helper for Axis-Aligned Bounding Box (AABB) collision detection
    bool checkAABBCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);

    // Helper to get color based on element type
    glm::vec4 getOrbColor(ElementType type) const {
        switch (type) {
        case EARTH: return glm::vec4(0.6f, 0.4f, 0.2f, 1.0f); // Brown
        case WATER: return glm::vec4(0.2f, 0.4f, 0.8f, 1.0f); // Blue
        case FIRE:  return glm::vec4(0.8f, 0.2f, 0.2f, 1.0f); // Red
        case AIR:   return glm::vec4(0.7f, 0.9f, 1.0f, 1.0f); // Light Blue/Cyan
        default:    return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Default white
        }
    }

public:
    Game(int width, int height); // Constructor
    ~Game();                     // Destructor

    void init(); // Initializes game objects and systems
    void update(float deltaTime, const glm::vec3& cameraPos); // Updates game logic
    void draw(GLuint gameShader, GLuint particleShader, const glm::mat4& view, const glm::mat4& projection); // Draws game elements
    void processInput(GLFWwindow* window, float deltaTime); // Handles player input
    void scrollCallback(double yoffset); // Handles mouse scroll input for basket type change
    void setScreenDimensions(int newWidth, int newHeight); // Updates game's internal screen dimensions
    void resetGame(); // Resets game to starting state

    int getScore() const { return score; } // Returns current score
    GameState getCurrentState() const { return m_currentState; } // Returns current game state

    // New getter to allow Orbs to access their specific particle system
    ParticleSystem* getParticleSystem(ElementType type) {
        if (type >= 0 && type < NUM_ELEMENT_TYPES) {
            return particleSystems[type].get();
        }
        return nullptr; // Should not happen if types are handled correctly
    }
};

// Game constructor: Initializes game state and objects.
Game::Game(int width, int height)
    : screenWidth(width), screenHeight(height), score(0),
    orbSpawnTimer(0.0f), orbSpawnInterval(1.5f), orbFallSpeed(100.0f),
    basketBottomMargin(30.0f), // Initial margin from the very bottom of the window
    rng(std::random_device{}()), // Initialize random number generator
    m_lastDestroyedOrbColor(1.0f, 1.0f, 1.0f, 1.0f),
    m_currentState(GameState::RUNNING) // Initialize game state
{
    // Initialize basket: centered horizontally, at the bottom of the screen (using new centered coords)
    // Y: bottom edge (-screenHeight/2) + half basket height + margin
    float basketY = -(static_cast<float>(screenHeight) / 2.0f) + (60.0f / 2.0f) + basketBottomMargin;
    playerBasket = std::make_unique<Basket>(
        0.0f, // Centered on X (0,0 is now center of screen)
        basketY,
        120.0f, // Basket width
        60.0f,  // Basket height
        300.0f  // Basket speed
    );

    // Initialize particle systems (one for each element type)
    particleSystems.resize(NUM_ELEMENT_TYPES);
    particleSystems[EARTH] = std::make_unique<ParticleSystem>(500, "textures/earth_particle.png");
    particleSystems[WATER] = std::make_unique<ParticleSystem>(500, "textures/water_particle.png");
    particleSystems[FIRE] = std::make_unique<ParticleSystem>(500, "textures/fire_particle.png");
    particleSystems[AIR] = std::make_unique<ParticleSystem>(500, "textures/air_particle.png");

    ma_result result = ma_engine_init(NULL, &m_audioEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl;
    }
    else {
        std::cout << "Audio engine initialized." << std::endl;
    }
}

// Game destructor (empty as unique_ptrs handle cleanup).
Game::~Game() {
    // m_digitTextures are raw GLuints, must be deleted manually
    for (GLuint texID : m_digitTextures) {
        if (texID != 0) {
            glDeleteTextures(1, &texID);
        }
    }
    if (m_minusTexture != 0) glDeleteTextures(1, &m_minusTexture); 
    if (m_gameOverTextureID != 0) glDeleteTextures(1, &m_gameOverTextureID);
    if (m_youWinTextureID != 0) glDeleteTextures(1, &m_youWinTextureID);
    if (m_pressRToRestartTextureID != 0) glDeleteTextures(1, &m_pressRToRestartTextureID);

    // m_scoreDigitQuad and m_messageQuad are GameObjects, their destructors will clean up their VAO/VBO.
    // playerBasket, fallingOrbs, particleSystems are unique_ptrs, they self-delete.


    ma_sound_uninit(&m_correctCatchSound);

    ma_sound_uninit(&m_wrongCatchSound);

    ma_engine_uninit(&m_audioEngine);
    std::cout << "Audio engine uninitialized." << std::endl;
}

// Initializes all game objects and particle systems.
void Game::init() {
    playerBasket->init();
    for (auto& ps : particleSystems) {
        ps->init();
    }

    m_scoreDigitQuad.init(); // Sets up VAO/VBO for a default quad

    // Load digit textures
    m_digitTextures.resize(10);
    for (int i = 0; i < 10; ++i) {
        std::string path = "textures/digits/" + std::to_string(i) + ".png";
        m_digitTextures[i] = loadTextureUtility(path.c_str());
        if (m_digitTextures[i] == 0) {
            std::cerr << "WARNING: Could not load digit texture: " << path << std::endl;
        }
    }
    m_minusTexture = loadTextureUtility("textures/digits/minus.png"); // Load minus texture (NEW)
    if (m_minusTexture == 0) {
        std::cerr << "WARNING: Could not load minus texture." << std::endl;
    }

    // Set scale for the digit quad once. Its position will change per digit.
    m_scoreDigitQuad.setScale(glm::vec3(m_digitWidth, m_digitHeight, 1.0f));
    // Initial color for digits (will be updated dynamically)
    m_scoreDigitQuad.setColor(m_lastDestroyedOrbColor);

    m_messageQuad.init();
    m_gameOverTextureID = loadTextureUtility("textures/you_lose.png");
    m_youWinTextureID = loadTextureUtility("textures/you_win.png");
    m_pressRToRestartTextureID = loadTextureUtility("textures/press_r_to_restart.png");

    ma_result result;

    result = ma_sound_init_from_file(&m_audioEngine, "sounds/correct_catch.wav", MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &m_correctCatchSound);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load correct catch sound: " << result << std::endl;
    }
    else {
        ma_sound_set_volume(&m_correctCatchSound, 0.5f); // Adjust volume if needed
        std::cout << "Correct catch sound loaded." << std::endl;
    }

    result = ma_sound_init_from_file(&m_audioEngine, "sounds/wrong_catch.wav", MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, &m_wrongCatchSound);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load wrong catch sound: " << result << std::endl;
    }
    else {
        ma_sound_set_volume(&m_wrongCatchSound, 0.5f); // Adjust volume if needed
        std::cout << "Wrong catch sound loaded." << std::endl;
    }
}

// Updates game logic for all elements.
void Game::update(float deltaTime, const glm::vec3& cameraPos) {
    if (m_currentState == GameState::RUNNING) {
        // Update falling orbs
        for (auto& orb : fallingOrbs) {
            orb->update(deltaTime, this); // Pass 'this' (pointer to the Game instance)
        }

        // Remove off-screen orbs and apply penalty
        fallingOrbs.erase(std::remove_if(fallingOrbs.begin(), fallingOrbs.end(),
            [this](const std::unique_ptr<Orb>& orb) {
                // Orb is off-screen if its bottom edge is below the screen's bottom edge (-screenHeight/2)
                if (orb->isOffScreen(-(static_cast<float>(screenHeight) / 2.0f))) {
                    score -= 2; // Penalty for missing an orb
                    m_lastDestroyedOrbColor = getOrbColor(orb->getType()); // Update last destroyed orb color
                    std::cout << "Orb missed! Score: " << score << std::endl;
                    ma_sound_start(&m_wrongCatchSound); // Play wrong sound for missed orb
                    return true; // Remove this orb
                }
                return false;
            }),
            fallingOrbs.end());

        // Spawn new orbs based on timer
        orbSpawnTimer += deltaTime;
        if (orbSpawnTimer >= orbSpawnInterval) {
            spawnOrb();
            orbSpawnTimer = 0.0f;
        }

        // Check for collisions between orbs and basket
        checkCollisions();

        // Update all particle systems
        for (auto& ps : particleSystems) {
            ps->update(deltaTime, cameraPos);
        }

        if (score <= -5) { // if score drops below -5
            m_currentState = GameState::GAME_OVER_LOSE;
            fallingOrbs.clear(); // Clear all existing orbs
            std::cout << "Game Over! You Lose!" << std::endl;
        }
        if (score >= 100) {
            m_currentState = GameState::GAME_OVER_WIN;
            fallingOrbs.clear(); // Clear all existing orbs
            std::cout << "You Win!" << std::endl;
        }

    }
    else { // Game is in GAME_OVER state
        for (auto& ps : particleSystems) {
            ps->update(deltaTime, cameraPos);
        }
    }
}

// Draws all game elements and particle systems.
void Game::draw(GLuint gameShader, GLuint particleShader, const glm::mat4& view, const glm::mat4& projection) {
    // Draw player basket using the main game shader (only if running)
    if (m_currentState == GameState::RUNNING) {
        playerBasket->draw(gameShader, view, projection);
    }

    // Draw falling orbs using the main game shader (only if running)
    for (auto& orb : fallingOrbs) {
        orb->draw(gameShader, view, projection);
    }

    // Draw particle systems using the dedicated particle shader
    for (auto& ps : particleSystems) {
        ps->draw(particleShader, view, projection);
    }

    m_scoreDigitQuad.setColor(m_lastDestroyedOrbColor); // Apply the last orb's color here!

    std::string scoreStr = std::to_string(std::abs(score)); // Use absolute value for digits

    // Calculate the rightmost X position for the score
    float currentX = (static_cast<float>(screenWidth) / 2.0f) - m_scoreDisplayMarginX;
    // Calculate the top Y position (centered vertically on the digit)
    float startY = (static_cast<float>(screenHeight) / 2.0f) - m_scoreDisplayMarginY - (m_digitHeight / 2.0f);

    // Adjust starting X to align the whole number to the right margin
    float totalScoreWidth = scoreStr.length() * m_digitWidth;
    if (score < 0) {
        totalScoreWidth += m_digitWidth * 0.7f; // Add space for minus sign, roughly 70% of digit width
    }
    currentX -= totalScoreWidth;

    // Draw the minus sign if score is negative
    if (score < 0) {
        m_scoreDigitQuad.textureID = m_minusTexture;
        m_scoreDigitQuad.setPosition(glm::vec3(currentX + (m_digitWidth * 0.35f), startY, 0.0f)); // Position it centered but smaller
        m_scoreDigitQuad.setScale(glm::vec3(m_digitWidth * 0.7f, m_digitHeight, 1.0f)); // Make it smaller/thinner
        m_scoreDigitQuad.draw(gameShader, view, projection);
        currentX += m_digitWidth * 0.7f; // Advance X after drawing minus
    }

    // Reset scale for digits after drawing minus sign (if it was drawn)
    m_scoreDigitQuad.setScale(glm::vec3(m_digitWidth, m_digitHeight, 1.0f));

    for (char digitChar : scoreStr) {
        int digitValue = digitChar - '0'; // Convert char '0' to int 0, '1' to 1, etc.
        if (digitValue >= 0 && digitValue < 10) {
            // Set the current digit's texture
            m_scoreDigitQuad.textureID = m_digitTextures[digitValue];

            // Set the position for the current digit (quad is centered at its position)
            m_scoreDigitQuad.setPosition(glm::vec3(currentX + m_digitWidth / 2.0f, startY, 0.0f));

            // Draw the digit using the game shader
            m_scoreDigitQuad.draw(gameShader, view, projection);

            // Move currentX for the next digit
            currentX += m_digitWidth;
        }
    }

    if (m_currentState != GameState::RUNNING) {
        m_messageQuad.setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // Ensure full color for messages

        // Draw Game Over/Win message
        m_messageQuad.setScale(glm::vec3(m_messageWidth, m_messageHeight, 1.0f));
        m_messageQuad.setPosition(glm::vec3(0.0f, 50.0f, 0.0f)); // Slightly above center
        if (m_currentState == GameState::GAME_OVER_LOSE) {
            m_messageQuad.textureID = m_gameOverTextureID;
        }
        else if (m_currentState == GameState::GAME_OVER_WIN) {
            m_messageQuad.textureID = m_youWinTextureID;
        }
        if (m_messageQuad.textureID != 0) {
            m_messageQuad.draw(gameShader, view, projection);
        }
        else {
            std::cerr << "Warning: Game Over/Win texture not loaded." << std::endl;
        }

        // Draw "Press R to Restart" message
        m_messageQuad.setScale(glm::vec3(m_restartMessageWidth, m_restartMessageHeight, 1.0f));
        m_messageQuad.setPosition(glm::vec3(0.0f, -50.0f, 0.0f)); // Below center
        m_messageQuad.textureID = m_pressRToRestartTextureID;
        if (m_messageQuad.textureID != 0) {
            m_messageQuad.draw(gameShader, view, projection);
        }
        else {
            std::cerr << "Warning: Restart texture not loaded." << std::endl;
        }
    }
}

// Processes keyboard input for basket movement.
void Game::processInput(GLFWwindow* window, float deltaTime) {
    if (m_currentState == GameState::RUNNING) {
        // Basket movement (A/D keys)
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            playerBasket->moveLeft(deltaTime);
            // Clamp basket to screen bounds (adjust for centered coordinates)
            if (playerBasket->getLeft() < -(static_cast<float>(screenWidth) / 2.0f)) {
                playerBasket->setPosition(glm::vec3(-(static_cast<float>(screenWidth) / 2.0f) + playerBasket->getScale().x / 2.0f, playerBasket->getPosition().y, 0.0f));
            }
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            playerBasket->moveRight(deltaTime);
            // Clamp basket to screen bounds (adjust for centered coordinates)
            if (playerBasket->getRight() > (static_cast<float>(screenWidth) / 2.0f)) {
                playerBasket->setPosition(glm::vec3((static_cast<float>(screenWidth) / 2.0f) - playerBasket->getScale().x / 2.0f, playerBasket->getPosition().y, 0.0f));
            }
        }
    }
    else { // Game is in GAME_OVER state
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            resetGame(); // Reset game if 'R' is pressed
        }
    }
}

// Handles mouse scroll input to change basket type.
void Game::scrollCallback(double yoffset) {
    if (m_currentState == GameState::RUNNING) { // Only allow type change if game is running
        if (yoffset > 0) {
            playerBasket->changeType(1); // Scroll up: next type
        }
        else if (yoffset < 0) {
            playerBasket->changeType(-1); // Scroll down: previous type
        }
    }
}

// Updates the game's internal screen dimensions and repositions elements as needed.
void Game::setScreenDimensions(int newWidth, int newHeight) {
    screenWidth = newWidth;
    screenHeight = newHeight;
    std::cout << "Game dimensions updated to: " << screenWidth << "x" << screenHeight << std::endl;

    // Recalculate basket's Y position to keep it a fixed distance from the new bottom edge
    float newBasketY = -(static_cast<float>(screenHeight) / 2.0f) + (playerBasket->getScale().y / 2.0f) + basketBottomMargin;
    playerBasket->setPosition(glm::vec3(playerBasket->getPosition().x, newBasketY, 0.0f));
    std::cout << "Basket Y position adjusted to: " << newBasketY << std::endl;
}

// Resets the game to its initial state.
void Game::resetGame() {
    score = 0;
    fallingOrbs.clear(); // Clear all existing orbs
    orbSpawnTimer = 0.0f;
    m_currentState = GameState::RUNNING;
    m_lastDestroyedOrbColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // Reset score color
    playerBasket->setPosition(glm::vec3(0.0f, -(static_cast<float>(screenHeight) / 2.0f) + (60.0f / 2.0f) + basketBottomMargin, 0.0f)); // Reset basket position
    playerBasket->setType(EARTH); // Reset basket type
    std::cout << "Game reset!" << std::endl;
}

// Spawns a new orb at a random X position at the top of the screen.
void Game::spawnOrb() {
    // Random X position: from left edge (-width/2) to right edge (width/2), with margins
    std::uniform_real_distribution<float> xDist(-(static_cast<float>(screenWidth) / 2.0f) + 30.0f, (static_cast<float>(screenWidth) / 2.0f) - 30.0f);
    std::uniform_int_distribution<int> typeDist(0, NUM_ELEMENT_TYPES - 1);

    ElementType randomType = static_cast<ElementType>(typeDist(rng));
    float randomX = xDist(rng);
    float orbSize = 60.0f; // Fixed orb size

    auto newOrb = std::make_unique<Orb>(
        randomX,
        static_cast<float>(screenHeight / 2.0f + orbSize / 2.0f), // Spawn the entire orb just above the screen's top edge
        orbSize,
        orbSize,
        randomType,
        orbFallSpeed
    );
    newOrb->init(); // Initialize its mesh and load texture
    fallingOrbs.push_back(std::move(newOrb));
    std::cout << "Orb spawned! Total orbs: " << fallingOrbs.size() << std::endl; // Debug output
}

// Checks for collisions between falling orbs and the player's basket.
void Game::checkCollisions() {
    auto& basket = playerBasket;

    // Iterate through orbs, removing those that collide
    for (auto it = fallingOrbs.begin(); it != fallingOrbs.end(); ) {
        Orb* orb = it->get();

        // Perform AABB collision check
        if (checkAABBCollision(
            orb->getLeft(), orb->getBottom(), orb->getScale().x, orb->getScale().y,
            basket->getLeft(), basket->getBottom(), basket->getScale().x, basket->getScale().y
        )) {
            // Collision detected!
            if (orb->getType() == basket->getType()) {
                score += 5; // Correct catch: increase score
                m_lastDestroyedOrbColor = getOrbColor(orb->getType()); // Update last destroyed orb color
                std::cout << "Correct catch! Score: " << score << std::endl;
                // Emit particles for correct catch
                particleSystems[orb->getType()]->emit(orb->getPosition(), 50, orb->getType()); // Emit 50 particles
                ma_sound_start(&m_correctCatchSound); // Play correct sound
            }
            else {
                score -= 2; // Wrong catch: decrease score
                m_lastDestroyedOrbColor = getOrbColor(orb->getType()); // Update last destroyed orb color
                std::cout << "Wrong catch! Score: " << score << std::endl;
                // Emit fewer particles for wrong catch
                particleSystems[orb->getType()]->emit(orb->getPosition(), 20, orb->getType()); // Fewer particles
                ma_sound_start(&m_wrongCatchSound); // Play wrong sound
            }
            it = fallingOrbs.erase(it); // Remove the orb
            continue; // Continue to next orb (iterator is already advanced by erase)
        }
        ++it; // Move to the next orb
    }
}

// Helper function for Axis-Aligned Bounding Box (AABB) collision detection.
bool Game::checkAABBCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2) {
    // Check for overlap on X-axis
    bool collisionX = x1 + w1 >= x2 && x2 + w2 >= x1;
    // Check for overlap on Y-axis
    bool collisionY = y1 + h1 >= y2 && y2 + h2 >= y1;
    // Collision occurs only if there is overlap on both axes
    return collisionX && collisionY;
}


// Base update: Does nothing by default, derived classes will override.
void GameObject::update(float deltaTime, Game* gameInstance) {}

// Basket update: Movement is input-driven, so no automatic update here.
void Basket::update(float deltaTime, Game* gameInstance) {
    // Basket movement is handled by processInput, so this update does nothing.
}

// Updates the orb's position (makes it fall) and emits particles.
void Orb::update(float deltaTime, Game* gameInstance) {
    position.y -= fallSpeed * deltaTime;

    position.x = m_initialX + m_zigzagAmplitude * sin(glfwGetTime() * m_zigzagFrequency + m_zigzagPhaseOffset);

    // Particle emission logic
    m_particleEmitTimer += deltaTime;
    if (m_particleEmitTimer >= m_particleEmitInterval) {
        if (gameInstance) { // Ensure gameInstance is not null
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> xDist(-width / 2.0f, width / 2.0f);
            std::uniform_real_distribution<float> yDist(-height / 2.0f, height / 2.0f);

            // Emit a small number of particles (e.g., 1) at a random position within the orb's area
            for (int i = 0; i < 1; ++i) {
                glm::vec3 emitPosition = position + glm::vec3(xDist(gen), yDist(gen), 0.0f);
                gameInstance->getParticleSystem(type)->emit(emitPosition, 1, type);
            }
        }
        m_particleEmitTimer = 0.0f; // Reset timer
    }
}



GLFWwindow* window; // GLFW window pointer
int current_width = 1024; // Current window width (dynamic)
int current_height = 768; // Current window height (dynamic)

// Orthographic camera for 2D. Positioned at (0,0,1) looking towards (0,0,0)
glm::vec3 cameraPos2D = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 cameraDir2D = glm::vec3(0.0f, 0.0f, -1.0f); // Looking into the screen
glm::vec3 cameraUp2D = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis is up

Camera camera(cameraPos2D, cameraDir2D, cameraUp2D); // Camera instance

float deltaTime = 0.0f; // Time between current and last frame
float lastFrame = 0.0f; // Time of last frame

std::unique_ptr<Game> game; // Game instance
GLuint gameShaderProgram;    // Shader program for game objects (basket, orbs)
GLuint particleShaderProgram; // Shader program for particle effects

// GLFW callback for window resize events
void window_callback(GLFWwindow* window, int new_width, int new_height)
{
    glViewport(0, 0, new_width, new_height); // Update OpenGL viewport
    current_width = new_width;   // Update global width
    current_height = new_height; // Update global height
    std::cout << "Window resized to: " << current_width << "x" << current_height << std::endl;

    // Inform the game instance about the new screen dimensions for internal logic adjustments
    if (game) {
        game->setScreenDimensions(new_width, new_height);
    }
}

// GLFW callback for mouse scroll events
void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (game) {
        game->scrollCallback(yoffset); // Pass scroll input to game for basket type change
    }
}

// Main function: Entry point of the application
int main(void)
{
    std::cout << "Starting main function..." << std::endl;

    // Initialize GLFW
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }
    std::cout << "GLFW initialized." << std::endl;

    // Configure OpenGL context version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create GLFW window and OpenGL context
    window = glfwCreateWindow(current_width, current_height, "Element Basket", NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to open GLFW window.");
        glfwTerminate();
        return -1;
    }
    std::cout << "GLFW window created and context requested." << std::endl;

    glfwMakeContextCurrent(window); // Make the window's context current on the calling thread
    std::cout << "OpenGL context made current." << std::endl;

    // Initialize GLEW (OpenGL Extension Wrangler Library)
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        glfwTerminate();
        return -1;
    }
    std::cout << "GLEW initialized." << std::endl;

    glViewport(0, 0, current_width, current_height); // Set initial OpenGL viewport
    std::cout << "Viewport set." << std::endl;

    glClearColor(0.2f, 0.3f, 0.5f, 1.0f); // Set background clear color

    // Enable blending for transparency (important for PNG textures with alpha)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // Disable depth testing for 2D game (draw order determines visibility)

    // Load shader programs
    gameShaderProgram = LoadShaders("SimpleVertexShader.vertexshader", "SimpleFragmentShader.fragmentshader");
    if (gameShaderProgram == 0) {
        std::cerr << "Failed to load game shaders! Exiting." << std::endl;
        glfwTerminate();
        return -1;
    }
    std::cout << "Game shaders loaded." << std::endl;

    particleShaderProgram = LoadShaders("ParticleVertexShader.vertexshader", "ParticleFragmentShader.fragmentshader");
    if (particleShaderProgram == 0) {
        std::cerr << "Failed to load particle shaders! Exiting." << std::endl;
        glDeleteProgram(gameShaderProgram); // Clean up already loaded game shader
        glfwTerminate();
        return -1;
    }
    std::cout << "Particle shaders loaded." << std::endl;

    // Create and initialize the Game instance
    game = std::make_unique<Game>(current_width, current_height);
    std::cout << "Game object created." << std::endl;
    game->init(); // This calls init() on all game objects and particle systems
    std::cout << "Game initialized." << std::endl;

    // Set GLFW callbacks
    glfwSetFramebufferSizeCallback(window, window_callback);
    glfwSetScrollCallback(window, mouse_scroll_callback);
    std::cout << "Callbacks set. Entering game loop." << std::endl;

    // Main game loop
    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        game->processInput(window, deltaTime);
        game->update(deltaTime, camera.position); // Pass camera position for particle updates

        glClear(GL_COLOR_BUFFER_BIT); // Clear the screen

        // Calculate view and projection matrices
        glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.viewDirection, camera.up);
        // Orthographic projection: centered (0,0) with dynamic width/height
        glm::mat4 projection = glm::ortho(-(float)current_width / 2.0f, (float)current_width / 2.0f,
            -(float)current_height / 2.0f, (float)current_height / 2.0f,
            0.1f, 100.0f);

        // Draw all game elements
        game->draw(gameShaderProgram, particleShaderProgram, view, projection);

        glfwSwapBuffers(window); // Swap front and back buffers
        glfwPollEvents();        // Process pending events (input, window resize, etc.)
    }

    // Cleanup resources before exiting
    std::cout << "Exiting game loop. Cleaning up." << std::endl;
    glDeleteProgram(gameShaderProgram);
    glDeleteProgram(particleShaderProgram);
    game.reset(); // Destroy game object and its components

    glfwTerminate(); // Terminate GLFW

    std::cout << "Program exited successfully." << std::endl;
    return 0;
}

