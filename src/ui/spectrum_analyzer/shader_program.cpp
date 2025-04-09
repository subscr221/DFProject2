#include "shader_program.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace df {
namespace ui {

ShaderProgram::ShaderProgram() : programId_(0) {
}

ShaderProgram::~ShaderProgram() {
    if (programId_ != 0) {
        glDeleteProgram(programId_);
        programId_ = 0;
    }
}

bool ShaderProgram::createFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    
    // Ensure ifstream objects can throw exceptions
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try {
        // Open files
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        
        // Read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        
        // Close file handlers
        vShaderFile.close();
        fShaderFile.close();
        
        // Convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
        return false;
    }
    
    return createFromStrings(vertexCode, fragmentCode);
}

bool ShaderProgram::createFromStrings(const std::string& vertexSource, const std::string& fragmentSource) {
    // Clean up any previous program
    if (programId_ != 0) {
        glDeleteProgram(programId_);
        programId_ = 0;
        uniformLocations_.clear();
    }
    
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return false;
    }
    
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }
    
    // Create program, attach shaders, and link
    programId_ = glCreateProgram();
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);
    glLinkProgram(programId_);
    
    // Check for linking errors
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(programId_, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(programId_, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(programId_);
        programId_ = 0;
        return false;
    }
    
    // Shaders are now linked, delete them
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return true;
}

void ShaderProgram::use() const {
    if (programId_ != 0) {
        glUseProgram(programId_);
    }
}

GLuint ShaderProgram::compileShader(GLenum shaderType, const std::string& source) {
    GLuint shader = glCreateShader(shaderType);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    
    // Check for compilation errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" 
                  << (shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") 
                  << "\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLint ShaderProgram::getUniformLocation(const std::string& name) const {
    // Check if we've already cached this uniform location
    auto it = uniformLocations_.find(name);
    if (it != uniformLocations_.end()) {
        return it->second;
    }
    
    // Get the location from OpenGL and cache it
    GLint location = glGetUniformLocation(programId_, name.c_str());
    if (location == -1) {
        std::cerr << "Warning: Uniform '" << name << "' not found in shader program." << std::endl;
    }
    
    // Cache the location (even if -1, to avoid repeatedly trying to find non-existent uniforms)
    const_cast<ShaderProgram*>(this)->uniformLocations_[name] = location;
    
    return location;
}

void ShaderProgram::setBool(const std::string& name, bool value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, static_cast<int>(value));
    }
}

void ShaderProgram::setInt(const std::string& name, int value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void ShaderProgram::setFloat(const std::string& name, float value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void ShaderProgram::setVec2(const std::string& name, const glm::vec2& value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2fv(location, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setVec3(const std::string& name, const glm::vec3& value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setVec4(const std::string& name, const glm::vec4& value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, 1, glm::value_ptr(value));
    }
}

void ShaderProgram::setMat4(const std::string& name, const glm::mat4& value) const {
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}

} // namespace ui
} // namespace df 