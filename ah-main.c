#include <stdlib.h>
#include <GL/glew.h>
#ifdef __APPLE__
#    include <GLUT/glut.h>
#else
#    include <GL/glut.h>
#endif
#include <math.h>
#include <stdio.h>
#include "ah-util.h"

#define unlikely(x)     x


/*
 * Global data used by our render callback:
 */

static struct {
    GLuint vertex_buffer, element_buffer;
    GLuint vertex_shader, fragment_shader, program;
    
    struct {
        GLint fade_factor;
    } uniforms;

    struct {
        GLint position;
    } attributes;

    GLfloat fade_factor;
} g_resources;

/*
 * Functions for creating OpenGL objects:
 */
static GLuint make_buffer(
    GLenum target,
    const void *buffer_data,
    GLsizei buffer_size
) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, buffer_size, buffer_data, GL_STATIC_DRAW);
    return buffer;
}

static void show_info_log(
    GLuint object,
    PFNGLGETSHADERIVPROC glGet__iv,
    PFNGLGETSHADERINFOLOGPROC glGet__InfoLog
)
{
    GLint log_length;
    char *log;

    glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
    log = malloc(log_length);
    glGet__InfoLog(object, log_length, NULL, log);
    fprintf(stderr, "%s", log);
    free(log);
}

static GLuint make_shader(GLenum type, const char *filename)
{
    GLint length;
    GLchar *source = file_contents(filename, &length);
    GLuint shader;
    GLint shader_ok;

    if (!source)
        return 0;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar**)&source, &length);
    free(source);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
    if (!shader_ok) {
        fprintf(stderr, "Failed to compile %s:\n", filename);
        show_info_log(shader, glGetShaderiv, glGetShaderInfoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint make_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLint program_ok;

    GLuint program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &program_ok);
    if (!program_ok) {
        fprintf(stderr, "Failed to link shader program:\n");
        show_info_log(program, glGetProgramiv, glGetProgramInfoLog);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

/*
 * Data used to seed our vertex array and element array buffers:
 */
#define N_VERTEX      7
#define HEX_RADIUS  1.0
#define M_SQRT3     1.7320508075688772
static const GLfloat g_vertex_buffer_data[] = { 
     HEX_RADIUS      ,                          0,
     HEX_RADIUS / 2.0, HEX_RADIUS * M_SQRT3 / 2.0,
    -HEX_RADIUS / 2.0, HEX_RADIUS * M_SQRT3 / 2.0,
    -HEX_RADIUS      ,                          0,
    -HEX_RADIUS / 2.0,-HEX_RADIUS * M_SQRT3 / 2.0,
     HEX_RADIUS / 2.0,-HEX_RADIUS * M_SQRT3 / 2.0,
     HEX_RADIUS      ,                          0,
};
static const GLushort g_element_buffer_data[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

/*
 * Load and create all of our resources:
 */
static int make_resources(void)
{
    g_resources.vertex_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        g_vertex_buffer_data,
        N_VERTEX * sizeof(GLfloat) * 2
    );
    g_resources.element_buffer = make_buffer(
        GL_ELEMENT_ARRAY_BUFFER,
        g_element_buffer_data,
        N_VERTEX * sizeof(GLushort)
    );

    g_resources.vertex_shader = make_shader(
        GL_VERTEX_SHADER,
        "ah.v.glsl"
    );
    if (g_resources.vertex_shader == 0)
        return 0;

    g_resources.fragment_shader = make_shader(
        GL_FRAGMENT_SHADER,
        "ah.f.glsl"
    );
    if (g_resources.fragment_shader == 0)
        return 0;

    g_resources.program = make_program(g_resources.vertex_shader, g_resources.fragment_shader);
    if (g_resources.program == 0)
        return 0;

    g_resources.uniforms.fade_factor
        = glGetUniformLocation(g_resources.program, "fade_factor");

    g_resources.attributes.position
        = glGetAttribLocation(g_resources.program, "position");

    return 1;
}

static int update_paused;

/*
 * GLUT callbacks:
 */
static unsigned int ah_posted_redisplays;
static void update_fade_factor(void)
{
    if (!update_paused) {
        int milliseconds = glutGet(GLUT_ELAPSED_TIME);
        g_resources.fade_factor = sinf((float)milliseconds * 0.001f) * 0.5f + 0.5f;
        ah_posted_redisplays++;
        glutPostRedisplay();
    }
}

static void update_fps(int seconds_counted)
{
    static int last_print;
    int i;

    glutTimerFunc(1000, update_fps, seconds_counted + 1);

    for (i = 0; i < last_print; i++) {
        fprintf(stderr, "\b");
    }
    last_print = fprintf(stderr, "FPS: %4u", ah_posted_redisplays / (seconds_counted + 1));
}

static void render(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_resources.program);

    glUniform1f(g_resources.uniforms.fade_factor, g_resources.fade_factor);
    
    glActiveTexture(GL_TEXTURE0);

    glBindBuffer(GL_ARRAY_BUFFER, g_resources.vertex_buffer);
    glVertexAttribPointer(
        g_resources.attributes.position,  /* attribute */
        2,                                /* size */
        GL_FLOAT,                         /* type */
        GL_FALSE,                         /* normalized? */
        sizeof(GLfloat)*2,                /* stride */
        (void*)0                          /* array buffer offset */
    );
    glEnableVertexAttribArray(g_resources.attributes.position);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_resources.element_buffer);
    glDrawElements(
        GL_TRIANGLE_FAN,    /* mode */
        N_VERTEX,           /* count */
        GL_UNSIGNED_SHORT,  /* type */
        (void*)0            /* element array buffer offset */
    );

    glDisableVertexAttribArray(g_resources.attributes.position);
    glutSwapBuffers();
}

static void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
    case 32:
        update_paused = !update_paused;
        break;
    case 27:
        fprintf(stderr, "\n");
        exit(0);
        break;
    default:
        break;
    }
}

/*
 * Entry point
 */
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(400, 400);
    glutCreateWindow("Hello World");
    glutIdleFunc(&update_fade_factor);
    glutTimerFunc(1000, update_fps, 0);
    glutDisplayFunc(&render);
    glutKeyboardFunc(&keyboard);

    glewInit();
    if (!GLEW_VERSION_2_0) {
        fprintf(stderr, "OpenGL 2.0 not available\n");
        return 1;
    }

    if (!make_resources()) {
        fprintf(stderr, "Failed to load resources\n");
        return 1;
    }

    glutMainLoop();
    return 0;
}

