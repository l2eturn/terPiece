#include "gfx/shader.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "shader: cannot open %s\n", path.c_str());
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Insert the defines right after the first line (#version ...)
static std::string withDefines(const std::string& src, const std::string& defines) {
    if (defines.empty()) return src;
    size_t nl = src.find('\n');
    if (nl == std::string::npos) return src + "\n" + defines;
    return src.substr(0, nl + 1) + defines + src.substr(nl + 1);
}

static GLuint compile(GLenum type, const std::string& src, const std::string& path) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader: %s\n%s\n", path.c_str(), log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Shader::~Shader() {
    if (prog_) glDeleteProgram(prog_);
}

bool Shader::load(const std::string& name, const std::string& defines) {
    name_ = name;
    defines_ = defines;
    std::string base = std::string(SHADER_DIR) + "/" + name;
    std::string vsrc, fsrc;
    if (!readFile(base + ".vert", vsrc) || !readFile(base + ".frag", fsrc)) return false;
    GLuint vs = compile(GL_VERTEX_SHADER, withDefines(vsrc, defines), base + ".vert");
    GLuint fs = compile(GL_FRAGMENT_SHADER, withDefines(fsrc, defines), base + ".frag");
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader link: %s\n%s\n", base.c_str(), log);
        glDeleteProgram(p);
        return false;
    }
    if (prog_) glDeleteProgram(prog_);
    prog_ = p;
    locs_.clear();
    return true;
}

GLint Shader::loc(const char* n) const {
    auto it = locs_.find(n);
    if (it != locs_.end()) return it->second;
    GLint l = glGetUniformLocation(prog_, n);
    locs_.emplace(n, l);
    return l;
}

void Shader::set(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::set(const char* n, const glm::mat3& m) const { glUniformMatrix3fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::set(const char* n, const glm::vec4& v) const { glUniform4fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, float f) const { glUniform1f(loc(n), f); }
void Shader::set(const char* n, int i) const { glUniform1i(loc(n), i); }
void Shader::setArray(const char* n, const glm::vec3* v, int count) const { glUniform3fv(loc(n), count, (const float*)v); }
void Shader::setArray(const char* n, const float* v, int count) const { glUniform1fv(loc(n), count, v); }

static std::unordered_map<std::string, std::unique_ptr<Shader>>& sharedShaders() {
    static std::unordered_map<std::string, std::unique_ptr<Shader>> m;
    return m;
}

Shader* sharedShader(const std::string& name) {
    auto& s = sharedShaders()[name];
    if (!s) {
        s = std::make_unique<Shader>();
        if (!s->load(name)) std::fprintf(stderr, "shader: %s failed to load\n", name.c_str());
    }
    return s.get();
}

void reloadSharedShaders() {
    for (auto& [name, s] : sharedShaders()) s->reload();
}
