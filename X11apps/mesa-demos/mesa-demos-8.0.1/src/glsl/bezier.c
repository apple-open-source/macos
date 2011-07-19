/*
 * Simple example for testing basic features of
 * geometry shaders
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include "shaderutil.h"

static const char *filename = "bezier.geom";

static GLuint fragShader;
static GLuint vertShader;
static GLuint geoShader;
static GLuint program;

#define QUIT 9999

GLfloat vertices[][3] =
   { {  -0.9, -0.9, 0.0 },
     {  -0.5,  0.9, 0.0 },
     {   0.5,  0.9, 0.0 },
     {   0.9, -0.9, 0.0 } };

GLfloat color[][4] =
{ { 1, 1, 1, 1 },
  { 1, 1, 1, 1 },
  { 1, 1, 1, 1 },
  { 1, 1, 1, 1 } };


static struct uniform_info Uniforms[] = {
   { "NumSubdivisions", 1, GL_INT, { 50, 0, 0, 0 }, -1 },
   END_OF_UNIFORMS
};

static void usage( char *name )
{
   fprintf(stderr, "usage: %s\n", name);
   fprintf(stderr, "\n" );
}


static void load_and_compile_shader(GLuint shader, const char *text)
{
   GLint stat;

   glShaderSource(shader, 1, (const GLchar **) &text, NULL);
   glCompileShader(shader);
   glGetShaderiv(shader, GL_COMPILE_STATUS, &stat);
   if (!stat) {
      GLchar log[1000];
      GLsizei len;
      glGetShaderInfoLog(shader, 1000, &len, log);
      fprintf(stderr, "bezier: problem compiling shader:\n%s\n", log);
      exit(1);
   }
}

static void read_shader(GLuint shader, const char *filename)
{
   const int max = 100*1000;
   int n;
   char *buffer = (char*) malloc(max);
   FILE *f = fopen(filename, "r");
   if (!f) {
      fprintf(stderr, "bezier: Unable to open shader file %s\n", filename);
      exit(1);
   }

   n = fread(buffer, 1, max, f);
   printf("bezier: read %d bytes from shader file %s\n", n, filename);
   if (n > 0) {
      buffer[n] = 0;
      load_and_compile_shader(shader, buffer);
   }

   fclose(f);
   free(buffer);
}

static void check_link(GLuint prog)
{
   GLint stat;
   glGetProgramiv(prog, GL_LINK_STATUS, &stat);
   if (!stat) {
      GLchar log[1000];
      GLsizei len;
      glGetProgramInfoLog(prog, 1000, &len, log);
      fprintf(stderr, "Linker error:\n%s\n", log);
   }
}

static void menu_selected(int entry)
{
   switch (entry) {
   case QUIT:
      exit(0);
      break;
   default:
      Uniforms[0].value[0] = entry;
   }

   SetUniformValues(program, Uniforms);
   glutPostRedisplay();
}


static void menu_init(void)
{
   glutCreateMenu(menu_selected);

   glutAddMenuEntry("1 Subdivision",  1);
   glutAddMenuEntry("2 Subdivisions", 2);
   glutAddMenuEntry("3 Subdivisions", 3);
   glutAddMenuEntry("4 Subdivisions", 4);
   glutAddMenuEntry("5 Subdivisions", 5);
   glutAddMenuEntry("6 Subdivisions", 6);
   glutAddMenuEntry("7 Subdivisions", 7);
   glutAddMenuEntry("10 Subdivisions", 10);
   glutAddMenuEntry("50 Subdivisions", 50);
   glutAddMenuEntry("100 Subdivisions", 100);
   glutAddMenuEntry("500 Subdivisions", 500);

   glutAddMenuEntry("Quit", QUIT);

   glutAttachMenu(GLUT_RIGHT_BUTTON);
}

static void init(void)
{
   static const char *fragShaderText =
      "void main() {\n"
      "    gl_FragColor = gl_Color;\n"
      "}\n";
   static const char *vertShaderText =
      "void main() {\n"
      "   gl_FrontColor = gl_Color;\n"
      "   gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
      "}\n";
   static const char *geoShaderText =
      "#version 120\n"
      "#extension GL_ARB_geometry_shader4 : enable\n"
      "void main()\n"
      "{\n"
      "  for(int i = 0; i < gl_VerticesIn; ++i)\n"
      "  {\n"
      "    gl_FrontColor = gl_FrontColorIn[i];\n"
      "    gl_Position = gl_PositionIn[i];\n"
      "    EmitVertex();\n"
      "  }\n"
      "}\n";


   if (!ShadersSupported())
      exit(1);

   menu_init();

   fragShader = glCreateShader(GL_FRAGMENT_SHADER);
   load_and_compile_shader(fragShader, fragShaderText);

   vertShader = glCreateShader(GL_VERTEX_SHADER);
   load_and_compile_shader(vertShader, vertShaderText);

   geoShader = glCreateShader(GL_GEOMETRY_SHADER_ARB);
   if (filename)
      read_shader(geoShader, filename);
   else
      load_and_compile_shader(geoShader,
                              geoShaderText);

   program = glCreateProgram();
   glAttachShader(program, vertShader);
   glAttachShader(program, geoShader);
   glAttachShader(program, fragShader);

   glProgramParameteriARB(program, GL_GEOMETRY_INPUT_TYPE_ARB,
                          GL_LINES_ADJACENCY_ARB);
   glProgramParameteriARB(program, GL_GEOMETRY_OUTPUT_TYPE_ARB,
                          GL_LINE_STRIP);

   {
      int temp;
      glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES_ARB,&temp);
      glProgramParameteriARB(program,GL_GEOMETRY_VERTICES_OUT_ARB,temp);
   }

   glLinkProgram(program);
   check_link(program);
   glUseProgram(program);

   SetUniformValues(program, Uniforms);
   PrintUniforms(Uniforms);

   assert(glGetError() == 0);

   glEnableClientState( GL_VERTEX_ARRAY );
   glEnableClientState( GL_COLOR_ARRAY );

   glVertexPointer( 3, GL_FLOAT, sizeof(vertices[0]), vertices );
   glColorPointer( 4, GL_FLOAT, sizeof(color[0]), color );
}

static void args(int argc, char *argv[])
{
   if (argc != 1) {
      usage(argv[0]);
      exit(1);
   }
}

static void Display( void )
{
   glClearColor(0, 0, 0, 1);
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   glUseProgram(program);

   glEnable(GL_VERTEX_PROGRAM_ARB);

   glDrawArrays(GL_LINES_ADJACENCY_ARB, 0, 4);

   glFlush();
}


static void Reshape( int width, int height )
{
   glViewport( 0, 0, width, height );
   glMatrixMode( GL_PROJECTION );
   glLoadIdentity();
   glOrtho(-1.0, 1.0, -1.0, 1.0, -0.5, 1000.0);
   glMatrixMode( GL_MODELVIEW );
   glLoadIdentity();
   /*glTranslatef( 0.0, 0.0, -15.0 );*/
}


static void CleanUp(void)
{
   glDeleteShader(fragShader);
   glDeleteShader(vertShader);
   glDeleteProgram(program);
}

static void Key( unsigned char key, int x, int y )
{
   (void) x;
   (void) y;
   switch (key) {
      case 27:
         CleanUp();
         exit(0);
         break;
   }
   glutPostRedisplay();
}

int main( int argc, char *argv[] )
{
   glutInit( &argc, argv );
   glutInitWindowPosition( 0, 0 );
   glutInitWindowSize( 250, 250 );
   glutInitDisplayMode( GLUT_RGB | GLUT_SINGLE | GLUT_DEPTH );
   glutCreateWindow(argv[0]);
   glewInit();
   glutReshapeFunc( Reshape );
   glutKeyboardFunc( Key );
   glutDisplayFunc( Display );
   args(argc, argv);
   init();
   glutMainLoop();
   return 0;
}
