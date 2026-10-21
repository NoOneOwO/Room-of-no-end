// GLWrappers.h
// Minimal RAII wrappers around the OpenGL objects used by the project
// (VAO, VBO/EBO, shader program) plus a small vertex-buffer mesh helper.
#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ir {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f};
};

class VertexArray {
public:
    VertexArray() { glGenVertexArrays(1, &id_); }
    ~VertexArray() {
        if (id_ != 0) {
            glDeleteVertexArrays(1, &id_);
        }
    }

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    VertexArray(VertexArray&& other) noexcept : id_(other.id_) { other.id_ = 0; }
    VertexArray& operator=(VertexArray&& other) noexcept {
        if (this != &other) {
            if (id_ != 0) {
                glDeleteVertexArrays(1, &id_);
            }
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    void bind() const { glBindVertexArray(id_); }
    GLuint id() const { return id_; }

private:
    GLuint id_ = 0;
};

class Buffer {
public:
    explicit Buffer(GLenum target) : target_(target) { glGenBuffers(1, &id_); }
    ~Buffer() {
        if (id_ != 0) {
            glDeleteBuffers(1, &id_);
        }
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept : id_(other.id_), target_(other.target_) { other.id_ = 0; }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            if (id_ != 0) {
                glDeleteBuffers(1, &id_);
            }
            id_ = other.id_;
            target_ = other.target_;
            other.id_ = 0;
        }
        return *this;
    }

    void bind() const { glBindBuffer(target_, id_); }
    void upload(const void* data, std::size_t bytes, GLenum usage = GL_STATIC_DRAW) {
        bind();
        glBufferData(target_, static_cast<GLsizeiptr>(bytes), data, usage);
    }
    GLuint id() const { return id_; }

private:
    GLuint id_ = 0;
    GLenum target_ = GL_ARRAY_BUFFER;
};

class Program {
public:
    Program() = default;
    ~Program() {
        if (id_ != 0) {
            glDeleteProgram(id_);
        }
    }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    // Compiles and links a vertex/fragment pair. Throws std::runtime_error with
    // the offending file name and the driver's info log on failure.
    void build(const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath) {
        const GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexPath);
        const GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentPath);

        const GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            std::cerr << "Program link error (" << vertexPath.string() << " + "
                      << fragmentPath.string() << "):\n" << log << '\n';
            glDeleteProgram(program);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            throw std::runtime_error("Failed to link shader program: " +
                                     vertexPath.filename().string() + " + " +
                                     fragmentPath.filename().string());
        }

        glDetachShader(program, vertexShader);
        glDetachShader(program, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        if (id_ != 0) {
            glDeleteProgram(id_);
        }
        id_ = program;
        locations_.clear();
    }

    void use() const { glUseProgram(id_); }

    GLint location(const char* name) const {
        const auto found = locations_.find(name);
        if (found != locations_.end()) {
            return found->second;
        }
        const GLint location = glGetUniformLocation(id_, name);
        locations_.emplace(name, location);
        return location;
    }

    void setInt(const char* name, int value) const { glUniform1i(location(name), value); }
    void setFloat(const char* name, float value) const {
        glUniform1f(location(name), value);
    }
    void setVec2(const char* name, const glm::vec2& value) const {
        glUniform2fv(location(name), 1, glm::value_ptr(value));
    }
    void setVec3(const char* name, const glm::vec3& value) const {
        glUniform3fv(location(name), 1, glm::value_ptr(value));
    }
    void setVec4(const char* name, const glm::vec4& value) const {
        glUniform4fv(location(name), 1, glm::value_ptr(value));
    }
    void setMat3(const char* name, const glm::mat3& value) const {
        glUniformMatrix3fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    void setMat4(const char* name, const glm::mat4& value) const {
        glUniformMatrix4fv(location(name), 1, GL_FALSE, glm::value_ptr(value));
    }

private:
    static GLuint compile(GLenum type, const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open shader file: " + path.string());
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        const std::string source = contents.str();
        if (source.empty()) {
            throw std::runtime_error("Shader file is empty: " + path.string());
        }

        const GLuint shader = glCreateShader(type);
        const char* sourcePtr = source.c_str();
        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            std::cerr << "Shader compile error (" << path.string() << "):\n" << log << '\n';
            glDeleteShader(shader);
            throw std::runtime_error("Failed to compile shader: " + path.filename().string());
        }
        return shader;
    }

    GLuint id_ = 0;
    mutable std::unordered_map<std::string, GLint> locations_;
};

// ------------------------------------------------------------------ textures
class Texture {
public:
    Texture() = default;
    ~Texture() { destroy(); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void create(int width, int height, GLenum internalFormat, GLenum format,
                GLenum type, GLenum filter) {
        destroy();
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width,
                     height, 0, format, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        width_ = width;
        height_ = height;
    }

    void destroy() {
        if (id_ != 0) {
            glDeleteTextures(1, &id_);
            id_ = 0;
        }
        width_ = 0;
        height_ = 0;
    }

    void bind(GLenum unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id_);
    }

    GLuint id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    GLuint id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

class Renderbuffer {
public:
    Renderbuffer() = default;
    ~Renderbuffer() { destroy(); }

    Renderbuffer(const Renderbuffer&) = delete;
    Renderbuffer& operator=(const Renderbuffer&) = delete;

    void create(int width, int height, GLenum format, int samples) {
        destroy();
        glGenRenderbuffers(1, &id_);
        glBindRenderbuffer(GL_RENDERBUFFER, id_);
        if (samples > 1) {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    void destroy() {
        if (id_ != 0) {
            glDeleteRenderbuffers(1, &id_);
            id_ = 0;
        }
    }

    GLuint id() const { return id_; }

private:
    GLuint id_ = 0;
};

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer() { destroy(); }

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void create() {
        destroy();
        glGenFramebuffers(1, &id_);
    }

    void destroy() {
        if (id_ != 0) {
            glDeleteFramebuffers(1, &id_);
            id_ = 0;
        }
    }

    void bind(GLenum target = GL_FRAMEBUFFER) const {
        glBindFramebuffer(target, id_);
    }

    void attachTexture(GLenum attachment, const Texture& texture) const {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
                               texture.id(), 0);
    }

    void attachRenderbuffer(GLenum attachment, const Renderbuffer& renderbuffer) const {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER,
                                  renderbuffer.id());
    }

    bool complete() const {
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }

    GLuint id() const { return id_; }

private:
    GLuint id_ = 0;
};

// Interleaved position/normal/color mesh with a VAO, VBO and EBO.
class Mesh {
public:
    void upload(const std::vector<Vertex>& vertices,
                const std::vector<std::uint32_t>& indices) {
        vao_.bind();
        vbo_.upload(vertices.data(), vertices.size() * sizeof(Vertex));
        ebo_.upload(indices.data(), indices.size() * sizeof(std::uint32_t));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(offsetof(Vertex, color)));

        vao_.bind();
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        indexCount_ = static_cast<GLsizei>(indices.size());
    }

    void draw() const {
        if (indexCount_ == 0) {
            return;
        }
        vao_.bind();
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void drawInstanced(GLsizei instances) const {
        if (indexCount_ == 0 || instances <= 0) {
            return;
        }
        vao_.bind();
        glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr,
                                instances);
        glBindVertexArray(0);
    }

    GLsizei indexCount() const { return indexCount_; }
    GLuint vaoId() const { return vao_.id(); }

private:
    VertexArray vao_;
    Buffer vbo_{GL_ARRAY_BUFFER};
    Buffer ebo_{GL_ELEMENT_ARRAY_BUFFER};
    GLsizei indexCount_ = 0;
};

} // namespace ir
