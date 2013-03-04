/* 
 * This code demonstrates erronious behavior of multisample renderbuffers on OSX
 * The bug occurs when a Multisample Renderbuffers above a certain size is allocated,
 * at which point it fails to allocate correctly and draw calls to it begin to fail silently.
 * Depending on the system this will either cause visual artifacts or possibly even crash.
 *
 * According to spec an OpenGL error should be raised if glRenderbufferStorageMultisample cannnot
 * allocate the requested size, but no such error is being raised here. 
 */

#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <ctime>
#include <cmath>
#include <cstdio>

// Increasing bufferSize or bufferCount may provoke the bug on systems where it does not occur initially
const GLsizei sampleCount = 4;
const GLsizei windowSize = 256;

bool hasError = false;
bool invalidFramebuffer = false;
bool maxSizeReached = false;

void CheckGLError(const char* id) {
    GLuint err = glGetError();
    if(err != GL_NO_ERROR) {
        printf("%s @ %s\n", gluErrorString(err), id);
        hasError = true;
    }
}

// This class encapsulates a Multisample Renderbuffer and a standard texture to blit the multisampled buffer to.
// It approximates the behavior of an Antialiased WebGL canvas in the Chrome compositor
class MultisampleBuffer {
    
public:
    MultisampleBuffer(GLsizei bufferWidth, GLsizei bufferHeight) {
        maxSize = 0;
        // Query the maximum size
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
        printf("Maximum reported renderbuffer size is: %d\n", maxSize);
        
        // Gen buffers
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &colorBuffer);
        
        glGenFramebuffers(1, &multisampleFBO);
        glGenRenderbuffers(1, &multisampleColorBuffer);
        glGenRenderbuffers(1, &multisampleDepthBuffer);
        
        // Setup Standard FBO
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferWidth, bufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
        
        // Setup Multisample FBO
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleColorBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_RGBA, bufferWidth, bufferHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColorBuffer);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepthBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT24, bufferWidth, bufferHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleDepthBuffer);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        width = bufferWidth;
        height = bufferHeight;
        
        CheckGLError("MultisampleBuffer created");
    }
    
    void Resize(GLsizei bufferWidth, GLsizei bufferHeight) {
        printf("Resizing buffer to %d x %d\n", bufferWidth, bufferHeight);
        
        if(bufferWidth > maxSize) {
            bufferWidth = maxSize;
            maxSizeReached = true;
        }
        if(bufferHeight > maxSize) {
            bufferHeight = maxSize;
            maxSizeReached = true;
        }
        
        width = bufferWidth;
        height = bufferHeight;
        
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleColorBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_RGBA, bufferWidth, bufferHeight);
        CheckGLError("multisampleColorBuffer resize");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, multisampleColorBuffer);
        
        glBindRenderbuffer(GL_RENDERBUFFER, multisampleDepthBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT16, bufferWidth, bufferHeight);
        CheckGLError("multisampleDepthBuffer resize");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, multisampleDepthBuffer);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            hasError = true;
            printf("Multisample Frambuffer reported as incomplete\n");
        } else {
            glBindTexture(GL_TEXTURE_2D, colorBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferWidth, bufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            CheckGLError("colorBuffer resize");
            glBindTexture(GL_TEXTURE_2D, 0);
            
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
            
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                hasError = true;
                printf("Color Frambuffer reported as incomplete\n");
            }
        }
    }
    
    // The frambuffer is tested by clearing it and drawing a quad into the center, then testing key pixel colors to ensure
    // they match the expected value.
    bool Test() {
        bool success = true;
        
        glBindFramebuffer(GL_FRAMEBUFFER, multisampleFBO);
        glViewport(0, 0, width, height);
        
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glColor3f(0.0, 0.0, 0.0);
        glBegin(GL_POLYGON);
            glVertex2f(-0.5, -0.5);
            glVertex2f(-0.5, 0.5);
            glVertex2f(0.5, 0.5);
            glVertex2f(0.5, -0.5);
        glEnd();
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        Commit();
        
        unsigned int pixel = 0;
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
        if(pixel != 0xFFFFFFFF) {
            printf("!!!Failed White Pixel Test!!!\n");
            success = false;
        }
        
        glReadPixels(width*0.5, height*0.5, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
        if(pixel != 0xFF000000) {
            printf("!!!Failed Black Pixel Test!!!\n");
            success = false;
        }
        
        return success;
    }
    
    // Copies the contents of the multisampled buffer to a texture so that it can be rendered to the screen
    void Commit() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
        CheckGLError("blit");
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    GLuint fbo;
    GLuint colorBuffer;
    GLuint width, height;
    
    GLint maxSize;
    
    GLuint multisampleFBO;
    GLuint multisampleColorBuffer;
    GLuint multisampleDepthBuffer;
};

MultisampleBuffer* buffer;
void InitScene()
{
    buffer = new MultisampleBuffer(windowSize, windowSize);
}

GLint winWidth, winHeight;
void Resize(GLint newWidth, GLint newHeight) {
    winWidth = newWidth;
    winHeight = newHeight;
}

// Draw a colored quad to the given multisample buffer
GLfloat ts = 0;
void DrawQuad(MultisampleBuffer* buffer) {
    ts += 1.0f;
    
    GLfloat r = (sinf(ts/10.0f) + 1.0f) * 0.5f;
    GLfloat g = (cosf(ts/100.0f) + 1.0f) * 0.5f;
    GLfloat b = (sinf(ts/1000.0f) + 1.0f) * 0.5f;
    
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->multisampleFBO);
    glViewport(0, 0, buffer->width, buffer->height);
    
    glClearColor(0.0, 0.0, 0.7, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(r, g, b);
	glBegin(GL_POLYGON);
        glVertex2f(-0.5, -0.5);
        glVertex2f(-0.5, 0.5);
        glVertex2f(0.5, 0.5);
        glVertex2f(0.5, -0.5);
	glEnd();
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    CheckGLError("Render to Framebuffer");
}

// Copies the given multisample buffer to a texture and renders that texture to the screen
void DrawBuffer(MultisampleBuffer* buffer) {
    // Copies the content of the multisampled buffer into a renderable texture
    buffer->Commit();
    
    glViewport(0, 0, winWidth, winHeight);
    
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, buffer->colorBuffer);
	glBegin(GL_POLYGON);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
	glEnd();
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    CheckGLError("Render to Screen");
}

void Render(void)
{
    glViewport(0, 0, winWidth, winHeight);
    
    if(hasError) {
        // If an OpenGL error has been encountered the screen will render solid green
        // Check the console output for error details
        glClearColor(0.0f, 1.0f, 0.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else if(invalidFramebuffer) {
        // If the framebuffer is invalid but no OpenGL errors are indicated, the screen will render solid red
        glClearColor(1.0f, 0.0f, 0.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else if(maxSizeReached) {
        // Render blue if we've successfully allocated a buffer of the maximum reported size without error
        glClearColor(0.0f, 0.0f, 1.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        // Steadily grow the multisample buffer size.
        // This is expected to fail at some point, since we should eventually hit memory or other limits.
        buffer->Resize(buffer->width * 1.2, buffer->height * 1.2);
        
        if(!hasError) {
            // Check the framebuffer to ensure it's valid and can be drawn to
            if(!buffer->Test()) {
                printf("Framebuffer appears to be invalid but no GL errors have been indicated. Should not get here!\n");
                invalidFramebuffer = true;
            }
            
            // Draws a quad into the center of the multisample buffer
            DrawQuad(buffer);
            
            // Draws the contents of the multisample buffer to the screen
            DrawBuffer(buffer);
        }
    }
    
    glutSwapBuffers();
    glutPostRedisplay();
}

int main(int argc, char** argv)
{
	glutInit(&argc,argv);
	glutInitDisplayMode (GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGB);
	glutInitWindowSize(windowSize, windowSize);
	glutInitWindowPosition(0,0);
	glutCreateWindow("Multisample Corruption (v2)");
    glutReshapeFunc(Resize);
    glutDisplayFunc(Render);

	InitScene();
	glutMainLoop();
}



