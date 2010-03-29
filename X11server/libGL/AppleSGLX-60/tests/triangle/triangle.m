#include <stdio.h>
#include <stdlib.h>
#include <GLUT/GLUT.h>
#include <OpenGL/OpenGL.h>

void initialize(void) {
    glEnable(GL_DEPTH_TEST);
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glutPostRedisplay();
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();	
    glBegin(GL_TRIANGLES);
    glVertex3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,-1.0f, 0.0f);
    glVertex3f( 1.0f,-1.0f, 0.0f);
    glEnd();	
    glFlush();
}

int main(int argc, char *argv[]) {
    int win;
    glutInit(&argc, (char **)argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH); 
    glutInitWindowSize(800, 600);
    win = glutCreateWindow("Test");
    
    initialize();

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);

    printf( "GL_RENDERER   = %s\n", (char *) glGetString( GL_RENDERER ) );
    printf( "GL_VERSION    = %s\n", (char *) glGetString( GL_VERSION ) );
    printf( "GL_VENDOR     = %s\n", (char *) glGetString( GL_VENDOR ) ) ;
    printf( "GL_EXTENSIONS = %s\n", (char *) glGetString( GL_EXTENSIONS ) );

    glutMainLoop();

    return EXIT_FAILURE;
}
