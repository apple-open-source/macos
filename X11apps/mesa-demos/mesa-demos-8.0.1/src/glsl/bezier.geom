#version 120
#extension GL_EXT_geometry_shader4: enable

uniform int NumSubdivisions;

void main()
{
   /* num is the number of subdivisions
    * can be anything between 1 and infinity
    */
   int num = NumSubdivisions;

   float dt = 1. / float(num);
   float t = 0.;
   for (int i = 0; i <= num; i++) {
      float omt = 1. - t;
      float omt2 = omt * omt;
      float omt3 = omt * omt2;
      float t2 = t * t;
      float t3 = t * t2;
      vec4 xyzw =
         omt3 * gl_PositionIn[0].xyzw +
         3. * t * omt2 * gl_PositionIn[1].xyzw +
         3. * t2 * omt * gl_PositionIn[2].xyzw +
         t3 * gl_PositionIn[3].xyzw;
      gl_Position = xyzw;
      gl_FrontColor = vec4(1, 1, 1, 1);
      EmitVertex();
      t += dt;
   }
}
