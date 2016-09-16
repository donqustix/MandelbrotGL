#include <GL/glew.h>

#include <SDL2/SDL.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <functional>
#include <utility>
#include <fstream>
#include <iterator>

namespace
{
    constexpr int WINDOW_WIDTH  = 800;
    constexpr int WINDOW_HEIGHT = 600;

    class GLobject
    {
    public:
        using deleter_type = std::function<void(GLuint)>;

    private:
        GLuint index;
        deleter_type deleter;

    public:
        GLobject(GLuint index, deleter_type deleter) : index{index}, deleter{std::move(deleter)} {}
        GLobject(const GLobject&) = delete;
        GLobject(GLobject&& globject) noexcept : index{globject.index}, deleter{std::move(globject.deleter)}
        {
            globject.index = 0;
        }
        ~GLobject() {deleter(index);}

        GLobject& operator=(const GLobject&) = delete;

        GLobject& operator=(GLobject&& globject) noexcept
        {
            if (this == &globject) return *this;

            index = globject.index;
            deleter = std::move(globject.deleter);

            return *this;
        }

        operator GLuint() const noexcept {return index;}
    };

    GLobject create_rectangle_buffer()
    {
        GLobject buffer
        {
            []
            {
                GLuint buffer;
                ::glGenBuffers(1, &buffer);

                if (!buffer)
                    throw std::runtime_error{"buffer generation error"};

                return buffer;

            }(), [](GLuint buffer){::glDeleteBuffers(1, &buffer);}
        };

        constexpr GLfloat rectangle_data[]
        {
            -1.0F, -1.0F, 0.0F,
             1.0F, -1.0F, 0.0F,
             1.0F,  1.0F, 0.0F,
             1.0F,  1.0F, 0.0F,
            -1.0F,  1.0F, 0.0F,
            -1.0F, -1.0F, 0.0F
        };

        ::glBindBuffer(GL_ARRAY_BUFFER, buffer);
        ::glBufferData(GL_ARRAY_BUFFER, sizeof(rectangle_data), rectangle_data, GL_STATIC_DRAW);
        ::glBindBuffer(GL_ARRAY_BUFFER, 0);

        return buffer;
    }

    GLobject create_rectangle_vertex_array_object(const GLobject& rectangle_buffer)
    {
        GLobject vertex_array_object
        {
            []
            {
                GLuint vertex_array_object;
                ::glGenVertexArrays(1, &vertex_array_object);

                if (!vertex_array_object)
                    throw std::runtime_error{"vertex array object generation error"};

                return vertex_array_object;

            }(), [](GLuint vertex_array_object) {::glDeleteVertexArrays(1, &vertex_array_object);}
        };

        ::glBindVertexArray(vertex_array_object);
        ::glBindBuffer(GL_ARRAY_BUFFER, rectangle_buffer);
        ::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
        ::glEnableVertexAttribArray(0);
        ::glBindBuffer(GL_ARRAY_BUFFER, 0);
        ::glBindVertexArray(0);

        return vertex_array_object;
    }

    GLobject create_shader(const std::string& source, GLenum shader_type)
    {
        GLobject shader
        {
            [shader_type]
            {
                const GLuint shader = ::glCreateShader(shader_type);
                if (!shader)
                    throw std::runtime_error{"shader creation error"};

                return shader;

            }(), [](GLuint shader) {::glDeleteShader(shader);}
        };

        const char* const source_cstr = source.c_str();
        ::glShaderSource(shader, 1, &source_cstr, nullptr);
        ::glCompileShader(shader);
        
        GLint status;
        ::glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE)
            throw std::runtime_error{"shader compilation error"};

        return shader;
    }

    GLobject create_shader_program(const std::string& vertex_shader_source, const std::string& fragment_shader_source)
    {
        GLobject shader_program
        {
            []
            {
                const GLuint shader_program = ::glCreateProgram();
                if (!shader_program)
                    throw std::runtime_error{"shader program creation error"};

                return shader_program;

            }(), [](GLuint shader_program) {::glDeleteProgram(shader_program);}
        };

        const GLobject vertex_shader{::create_shader(vertex_shader_source, GL_VERTEX_SHADER)};
        const GLobject fragment_shader{::create_shader(fragment_shader_source, GL_FRAGMENT_SHADER)};

        ::glAttachShader(shader_program, vertex_shader);
        ::glAttachShader(shader_program, fragment_shader);
        ::glLinkProgram(shader_program);

        GLint status;
        ::glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
        if (status == GL_FALSE)
            throw std::runtime_error{"failed to link shader program"};

        ::glDetachShader(shader_program, vertex_shader);
        ::glDetachShader(shader_program, fragment_shader);

        return shader_program;
    }

    std::string read_file(const std::string& file_path)
    {
        std::ifstream stream{file_path, std::ios::in | std::ios::binary};
        if (!stream)
            throw std::runtime_error{"file reading error"};

        return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    }
}

int main()
{
    if (::SDL_Init(SDL_INIT_VIDEO) >= 0)
    {
        ::SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        ::SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        ::SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        ::SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        SDL_Window* const window = ::SDL_CreateWindow("MandelbrotGL",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
        if (window)
        {
            const SDL_GLContext gl_context = ::SDL_GL_CreateContext(window);
            if (gl_context)
            {
                ::glewExperimental = GL_TRUE;
                ::glewInit();

                try
                {
                    const GLobject rectangle_buffer{::create_rectangle_buffer()};
                    const GLobject rectangle_vertex_array_object{
                                ::create_rectangle_vertex_array_object(rectangle_buffer)};

                    const GLobject shader_program{::create_shader_program(::read_file("res/mandelbrot_shader.vs"),
                                                                          ::read_file("res/mandelbrot_shader.fs"))};

                    unsigned max_iterations = 30;
                    float scale = 1.0F;
                    float x = 0.0F, y = 0.0F;

                    SDL_Event event;

                    bool running = true;
                    while (running)
                    {
                        while (::SDL_PollEvent(&event))
                        {
                            switch (event.type)
                            {
                                case SDL_KEYDOWN:
                                {
                                    const SDL_Scancode scancode = event.key.keysym.scancode;

                                    const float move_delta = 0.1F / scale;

                                    if (scancode == SDL_SCANCODE_W)
                                        y += move_delta;
                                    else if (scancode == SDL_SCANCODE_S)
                                        y -= move_delta;
                                    if (scancode == SDL_SCANCODE_A)
                                        x -= move_delta;
                                    else if (scancode == SDL_SCANCODE_D)
                                        x += move_delta;
                                    if (scancode == SDL_SCANCODE_Z)
                                        scale += 0.1F * scale;
                                    else if (scancode == SDL_SCANCODE_X)
                                        scale -= 0.1F * scale;
                                    if (scancode == SDL_SCANCODE_R)
                                        ++max_iterations;
                                    else if (scancode == SDL_SCANCODE_E)
                                        if (max_iterations > 0) --max_iterations;

                                    break;
                                }
                                case SDL_QUIT:
                                    running = false;
                                    break;
                            }
                        }

                        ::glUseProgram(shader_program);
                        ::glUniform1f(0, WINDOW_WIDTH);
                        ::glUniform1f(1, WINDOW_HEIGHT);
                        ::glUniform2f(2, -2.0F / scale + x, 1.0F / scale + x);
                        ::glUniform2f(3, -1.0F / scale + y, 1.0F / scale + y);
                        ::glUniform1ui(4, max_iterations);

                        ::glClear(GL_COLOR_BUFFER_BIT);

                        ::glBindVertexArray(rectangle_vertex_array_object);
                        ::glDrawArrays(GL_TRIANGLES, 0, 6);
                        ::glBindVertexArray(0);

                        ::glUseProgram(0);

                        ::SDL_GL_SwapWindow(window);
                        ::SDL_Delay(30);
                    }
                }
                catch(const std::exception& ex)
                {
                    std::cerr << ex.what() << std::endl;
                }

                ::SDL_GL_DeleteContext(gl_context);
            }
            else
                std::cerr << "SDL_GLContext creation error: " << ::SDL_GetError() << std::endl;

            ::SDL_DestroyWindow(window);
        }
        else
            std::cerr << "SDL_Window creation error: " << ::SDL_GetError() << std::endl;

        ::SDL_Quit();
    }
    else
        std::cerr << "SDL2 initialization error: " << ::SDL_GetError() << std::endl;

    return 0;
}
