#version 120
#extension GL_ARB_geometry_shader4 : enable

void main()
{
  for(int i = 0; i < gl_VerticesIn; ++i)
  {
    gl_FrontColor = gl_FrontColorIn[i];
    gl_Position = gl_PositionIn[i];
    EmitVertex();
  }
  EndPrimitive();
}
