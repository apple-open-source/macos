#include "StringDict.h"
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include "colors.h"
using namespace std;

typedef struct hsbcolor_t {
	char			*name;
	unsigned char	h,s,b;
} hsbcolor_t;

#include "../../tools/src/colortbl.h"

typedef map<DString,Color> named_colors;
named_colors g_namedColors;
void initNamed() {
    for(unsigned int i = 0; i<sizeof(color_lib)/sizeof(hsbcolor_t); ++i)
        g_namedColors[color_lib[i].name] = Color(color_lib[i].h/255.0f,color_lib[i].s/255.0f,color_lib[i].b/255.0f);
}
Color findColor(DString s) {
    if(g_namedColors.empty())
        initNamed();
    named_colors::iterator ci = g_namedColors.find(s);
    if(ci==g_namedColors.end())
        throw ColorNotFound(s);
    return ci->second;
}
