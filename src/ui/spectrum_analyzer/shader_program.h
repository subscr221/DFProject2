#pragma once

#include <string>
#include <unordered_map>
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace df {
namespace ui {

/**
 * @class ShaderProgram
 * @brief Handles compilation, linking, and management of OpenGL shader programs
 */
class ShaderProgram {
public:
    /**
     * @brief Default constructor
     */
    ShaderProgram();

    /**
     * @brief Destructor - cleans up shader program
     */
    ~ShaderProgram();

    /**
     * @brief Creates a shader program from vertex and fragment shader source files
     * 
     * @param vertexPath Path to the vertex shader source file
     * @param fragmentPath Path to the fragment shader source file
     * @return bool True if shader compilation and linking succeeded
     */
    bool createFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    /**
     * @brief Creates a shader program from vertex and fragment shader source strings
     * 
     * @param vertexSource Vertex shader source code as string
     * @param fragmentSource Fragment shader source code as string
     * @return bool True if shader compilation and linking succeeded
     */
    bool createFromStrings(const std::string& vertexSource, const std::string& fragmentSource);

    /**
     * @brief Use this shader program for rendering
     */
    void use() const;

    /**
     * @brief Get the program ID
     * 
     * @return GLuint OpenGL program ID
     */
    GLuint getProgramId() const { return programId_; }

    /**
     * @brief Set a boolean uniform
     * 
     * @param name Uniform name in shader
     * @param value Boolean value
     */
    void setBool(const std::string& name, bool value) const;

    /**
     * @brief Set an integer uniform
     * 
     * @param name Uniform name in shader
     * @param value Integer value
     */
    void setInt(const std::string& name, int value) const;

    /**
     * @brief Set a float uniform
     * 
     * @param name Uniform name in shader
     * @param value Float value
     */
    void setFloat(const std::string& name, float value) const;

    /**
     * @brief Set a vec2 uniform
     * 
     * @param name Uniform name in shader
     * @param value Vec2 value
     */
    void setVec2(const std::string& name, const glm::vec2& value) const;

    /**
     * @brief Set a vec3 uniform
     * 
     * @param name Uniform name in shader
     * @param value Vec3 value
     */
    void setVec3(const std::string& name, const glm::vec3& value) const;

    /**
     * @brief Set a vec4 uniform
     * 
     * @param name Uniform name in shader
     * @param value Vec4 value
     */
    void setVec4(const std::string& name, const glm::vec4& value) const;

    /**
     * @brief Set a mat4 uniform
     * 
     * @param name Uniform name in shader
     * @param value Mat4 value
     */
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    /**
     * @brief Compile a shader from source
     * 
     * @param shaderType Type of shader (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER)
     * @param source Shader source code
     * @return GLuint Shader ID
     */
    GLuint compileShader(GLenum shaderType, const std::string& source);

    /**
     * @brief Get the location of a uniform variable
     * 
     * @param name Uniform name
     * @return GLint Location of the uniform or -1 if not found
     */
    GLint getUniformLocation(const std::string& name) const;

    GLuint programId_ = 0; ///< OpenGL program ID
    std::unordered_map<std::string, GLint> uniformLocations_; ///< Cache for uniform locations
};

} // namespace ui
} // namespace df 