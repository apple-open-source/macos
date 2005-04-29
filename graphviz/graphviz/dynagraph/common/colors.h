struct HSVColor {
	float h,s,v;
};
struct RGBAColor {
    int r,g,b,a;
};
struct ColorNotFound : DGException { DString s; ColorNotFound(DString s) : DGException("color not found"),s(s) {} };
struct Color;
Color findColor(DString s);
struct Color {
    bool isHSV;
    union {
        HSVColor hsv;
        RGBAColor rgba;
    };
    Color() : isHSV(false) {
        rgba.r = rgba.g = rgba.b = 0;
        rgba.a = -1;
    }
    Color(int r,int g,int b,int a = 255) {
        isHSV = false;
        rgba.r = r;
        rgba.g = g;
        rgba.b = b;
        rgba.a = a;
    }
    Color(float h,float s,float v) {
        isHSV = true;
        hsv.h = h;
        hsv.s = s;
        hsv.v = v;
    }
    Color(DString s) {
        const char *c = s.c_str();
        while(isspace(*c)) ++c;
        isHSV = false;
        if(!(*c=='#')) {
            if(isdigit(*c)) {
                isHSV = true;
                sscanf(c,"%f %f %f",&hsv.h,&hsv.s,&hsv.v);
            }
            else 
                *this = findColor(c);
        }
        else {
            int n = sscanf(c+1,"%2x%2x%2x",&rgba.r,&rgba.g,&rgba.b) + c-s.c_str()+1;
            if(s[n] && (isdigit(s[n]) || isalpha(s[n]) && s[n]<'g'))
                sscanf(s.c_str()+n,"%2x",&rgba.a);
        }
    }
    DString toString() {
        char buf[200];
        if(isHSV)
            sprintf(buf,"%f %f %f",hsv.h,hsv.s,hsv.v);
        else
            if(rgba.a>=0)
                sprintf(buf,"%2x%2x%2x%2x",rgba.r,rgba.g,rgba.b,rgba.a);
            else
                sprintf(buf,"%2x%2x%2x",rgba.r,rgba.g,rgba.b);
        return buf;
    }
    // thanks to http://www.cs.rit.edu/~ncs/color/t_convert.html
    Color toHSV() {
        if(isHSV)
            return *this;
        Color ret;
        ret.isHSV = true;
	    float r = rgba.r/255.0f,g = rgba.g/255.0f,b = rgba.b/255.0f,
			low = std::min(r,std::min(g, b)),
	        high = std::max(r,std::max(g, b));
	    ret.hsv.v = high;				// v
	    float delta = high - low;
	    if(high != 0)
		    ret.hsv.s = delta / high;		// s
	    else {
		    // r = g = b = 0		// s = 0, v is undefined
		    ret.hsv.s = 0;
		    ret.hsv.h = -1;
		    return ret;
	    }
	    if(r == high)
		    ret.hsv.h = (g - b) / delta;		// between yellow & magenta
	    else if(g == high)
		    ret.hsv.h = 2 + (b - r) / delta;	// between cyan & yellow
	    else
		    ret.hsv.h = 4 + (r - g) / delta;	// between magenta & cyan
	    ret.hsv.h *= 60;				// degrees
	    if(ret.hsv.h < 0)
		    ret.hsv.h += 360;
        ret.hsv.h /= 360;
        return ret;
    }
    Color toRGB() {
        if(!isHSV)
            return *this;
        Color ret;
	    if(hsv.s == 0) {
		    // achromatic (grey)
		    ret.rgba.r = ret.rgba.g = ret.rgba.b = int(hsv.v*255.0f);
		    return ret;
	    }
	    float h = hsv.h*6;			// sector 0 to 5
	    int i = int(h);
	    float f = h - i,			// factorial part of h
	        p = hsv.v * (1 - hsv.s),
	        q = hsv.v * (1 - hsv.s * f),
	        t = hsv.v * (1 - hsv.s * (1 - f));
        int vi = int(hsv.v*255.0f),
            pi = int(p*255.0f),
            qi = int(q*255.0f),
            ti = int(t*255.0f);
	    switch(i) {
		    case 0:
			    ret.rgba.r = vi;
			    ret.rgba.g = ti;
			    ret.rgba.b = pi;
			    break;
		    case 1:
			    ret.rgba.r = qi;
			    ret.rgba.g = vi;
			    ret.rgba.b = pi;
			    break;
            case 2:
			    ret.rgba.r = pi;
			    ret.rgba.g = vi;
			    ret.rgba.b = ti;
			    break;
		    case 3:
			    ret.rgba.r = pi;
			    ret.rgba.g = qi;
			    ret.rgba.b = vi;
			    break;
		    case 4:
			    ret.rgba.r = ti;
			    ret.rgba.g = pi;
			    ret.rgba.b = vi;
			    break;
		    default:		// case 5:
			    ret.rgba.r = vi;
			    ret.rgba.g = pi;
			    ret.rgba.b = qi;
			    break;
	    }
        return ret;
    }
};
