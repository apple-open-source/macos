/* objplane.h - structure values for plane */

{       "plane", "plane",       /* long and short names */
        "cube",         /* long name of dual */
        4, 4, 1,        /* number of vertices, edges, and faces */
        {               /* vertices (x,y,z) */
                        /* all points must be within radius 1 of the origin */
#define T 1.0
                {  T,  0,  0 },
                { -T,  0,  0 },
                {  0,  T,  0 },
                {  0, -T,  0 },
#undef T
        },
        {       /* faces (numfaces + indexes into vertices) */
                /*  faces must be specified clockwise from the outside */
                4,      0, 2, 1, 3,
        }
},              /* leave a comma to separate from the next include file */
/* end */
