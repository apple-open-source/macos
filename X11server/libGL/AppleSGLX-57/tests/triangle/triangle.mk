triangle: tests/triangle/triangle.m libgl.a libglx.a
	$(CC) tests/triangle/triangle.m -framework GLUT -framework OpenGL -o triangle
