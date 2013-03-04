/* 
 * This code demonstrates a graphical corruption bug that affects certain NVIDIA chipsets on OSX
 * The bug occurs with Multisample Renderbuffers when there is a certain amount of VRAM pressure,
 * at which point Renderbuffers that haven't been drawn to in a while collect garbage data, which
 * can be seen when blitting the Renderbuffer to a texture.
 */

#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <ctime>
#include <cmath>
#include <cstdio>

// Increasing bufferSize or bufferCount may provoke the bug on systems where it does not occur initially
const GLsizei sampleCount = 4;
const GLsizei bufferSize = 512;
const GLsizei bufferCount = 215;

void CheckGLError(const char* id) {
    GLuint err = glGetError();
    if(err != GL_NO_ERROR) {
        printf("%s @ %s\n", gluErrorString(err), id);
    }
}

// This class encapsulates a Multisample Renderbuffer and a standard texture to blit the multisampled buffer to.
// It approximates the behavior of an Antialiased WebGL canvas in the Chrome compositor
class MultisampleBuffer {
    
public:
    MultisampleBuffer() {
        // Gen buffers
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &colorBuffer);
        
        glGenFramebuffers(1, &multisampleFBO);
        glGenRenderbuffers(1, &multisampleColorBuffer);
        glGenRenderbuffers(1, &multisampleDepthBuffer);
        
        // Setup Standard FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize, bufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
        
        // Setup Multisample FBO
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleColorBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_RGBA, bufferSize, bufferSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColorBuffer);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepthBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT24, bufferSize, bufferSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleDepthBuffer);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    // Copies the contents of the multisampled buffer to a texture so that it can be rendered to the screen
    void Commit() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        
        glBlitFramebuffer(0, 0, bufferSize, bufferSize, 0, 0, bufferSize, bufferSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
        CheckGLError("blit");
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    GLuint fbo;
    GLuint colorBuffer;
    
    GLuint multisampleFBO;
    GLuint multisampleColorBuffer;
    GLuint multisampleDepthBuffer;
};

// Draw a colored quad to the given multisample buffer
GLfloat ts = 0;
void DrawQuad(MultisampleBuffer* buffer) {
    ts += 1.0f;
    
    GLfloat r = (sinf(ts/10.0f) + 1.0f) * 0.5f;
    GLfloat g = (cosf(ts/100.0f) + 1.0f) * 0.5f;
    GLfloat b = (sinf(ts/1000.0f) + 1.0f) * 0.5f;
    
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->multisampleFBO);
    glViewport(0, 0, bufferSize, bufferSize);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(r, g, b);
	glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
	glEnd();
    
    CheckGLError("draw quad");
}

// Copies the given multisample buffer to a texture and renders that texture to the screen (either the left or right half)
void DrawBuffer(MultisampleBuffer* buffer, bool isStatic) {
    buffer->Commit();
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, buffer->colorBuffer);
	glBegin(GL_POLYGON);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(isStatic ? -1.0f : 0.0f, -1.0);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(isStatic ? -1.0f : 0.0f, 1.0);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(isStatic ? 0.0f : 1.0f, 1.0);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(isStatic ? 0.0f : 1.0f, -1.0);
	glEnd();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    CheckGLError("draw buffer");
}

// The application maintains a "static" Multisample buffer, which is drawn to once and never written to again
// and an array of "dynamic" buffers, which are drawn to continuously while the program runs to create VRAM pressure

MultisampleBuffer* buffers[bufferCount];
MultisampleBuffer* staticBuffer;
void InitScene()
{
    staticBuffer = new MultisampleBuffer();
    
    for(int i = 0; i < bufferCount; ++i) {
        buffers[i] = new MultisampleBuffer();
    }
    
    // Here the static buffer has a quad drawn to it, this is the only time that we write content to this buffer
    glClearColor (1.0, 0.0, 0.0, 0.0);
    DrawQuad(staticBuffer);
    glClearColor (0.0, 0.0, 1.0, 0.0);
}

GLint winWidth, winHeight;
void Resize(GLint newWidth, GLint newHeight) {
    winWidth = newWidth;
    winHeight = newHeight;
}

int lastBuffer = 0;
void Render(void)
{
    // Each frame one of the dynamic buffers is selected and a new colored quad is drawn to it
    // The color cycles with time so that the dynamic buffers can be visually identified
    int bufferId = lastBuffer % bufferCount;
    DrawQuad(buffers[bufferId]);
    
    // Each frame the newly updated dynamic buffer is drawn to the right half of the window (with a blue background)
    // and the static buffers is drawn to the left half (with a red background). While the static buffer is blitted
    // to texture each time, the multisample buffer is unchanged by this application.
    glViewport(0, 0, winWidth, winHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    DrawBuffer(buffers[bufferId], false);
    DrawBuffer(staticBuffer, true);
    
    glutSwapBuffers();
    glutPostRedisplay();
    
    lastBuffer++;
}

int main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutInitDisplayMode (GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGB);
	glutInitWindowSize(bufferSize * 2, bufferSize);
	glutInitWindowPosition(0,0);
	glutCreateWindow("Multisample Corruption");
    glutReshapeFunc(Resize);
    glutDisplayFunc(Render);

	InitScene();
	glutMainLoop();
}



