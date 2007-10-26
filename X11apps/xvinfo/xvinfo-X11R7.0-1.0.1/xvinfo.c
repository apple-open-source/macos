/* $XFree86: xc/programs/xvinfo/xvinfo.c,v 1.7 2001/10/28 03:34:43 tsi Exp $ */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void
PrintUsage(void)
{
   fprintf(stderr, "Usage:  xvinfo [-display host:dpy]\n");
   exit(0);
}

int main(int argc, char *argv[])
{
    Display *dpy;
    unsigned int ver, rev, eventB, reqB, errorB; 
    int i, j, k, n; 
    unsigned int nencode, nadaptors;
    int nscreens, nattr, numImages;
    XvAdaptorInfo *ainfo;
    XvAttribute *attributes;
    XvEncodingInfo *encodings;
    XvFormat *format;
    XvImageFormatValues *formats;
    char * disname = NULL;

    if((argc != 1) && (argc != 3))
       PrintUsage();

    if(argc != 1) {
       if(strcmp(argv[1], "-display"))
	  PrintUsage();
	disname = argv[2];
    }


    if(!(dpy = XOpenDisplay(disname))) 
    {
	fprintf(stderr, "xvinfo:  Unable to open display %s\n",
                       (disname != NULL) ? disname : XDisplayName(NULL));
	exit(-1);
    }

    if((Success != XvQueryExtension(dpy, &ver, &rev, &reqB, &eventB, &errorB))) 
    {
	fprintf(stderr, "xvinfo: No X-Video Extension on %s\n",
                        (disname != NULL) ? disname : XDisplayName(NULL));
	exit(0);
    } 
    else
    {
	fprintf(stdout, "X-Video Extension version %i.%i\n", ver, rev);
    }

    nscreens = ScreenCount(dpy);

    for(i = 0; i < nscreens; i++) {
  	fprintf(stdout, "screen #%i\n", i);
	XvQueryAdaptors(dpy, RootWindow(dpy, i), &nadaptors, &ainfo);

	if(!nadaptors) {
	    fprintf(stdout, " no adaptors present\n");
	    continue;
	} 

	for(j = 0; j < nadaptors; j++) {
	    fprintf(stdout, "  Adaptor #%i: \"%s\"\n", j, ainfo[j].name);
	    fprintf(stdout, "    number of ports: %li\n", ainfo[j].num_ports);
	    fprintf(stdout, "    port base: %li\n", ainfo[j].base_id);
	    fprintf(stdout, "    operations supported: ");
	    switch(ainfo[j].type & (XvInputMask | XvOutputMask)) {
	    case XvInputMask:
	     if(ainfo[j].type & XvVideoMask) 
		fprintf(stdout, "PutVideo ");
	     if(ainfo[j].type & XvStillMask) 
		fprintf(stdout, "PutStill ");
	     if(ainfo[j].type & XvImageMask) 
		fprintf(stdout, "PutImage ");
	     break;
	    case XvOutputMask:
	     if(ainfo[j].type & XvVideoMask) 
		fprintf(stdout, "GetVideo ");
	     if(ainfo[j].type & XvStillMask) 
		fprintf(stdout, "GetStill ");
	     break;
	    default:
		fprintf(stdout, "none ");
		break;
	    }
	    fprintf(stdout, "\n");

	    format = ainfo[j].formats;

	    fprintf(stdout, "    supported visuals:\n");
	    for(k = 0; k < ainfo[j].num_formats; k++, format++) {
	         fprintf(stdout, "      depth %i, visualID 0x%2lx\n",
				 format->depth, format->visual_id);
	    }

	    attributes = XvQueryPortAttributes(dpy, ainfo[j].base_id, &nattr);

	    if(attributes && nattr) {
		fprintf(stdout, "    number of attributes: %i\n", nattr);
		
		for(k = 0; k < nattr; k++) {
		    fprintf(stdout, "      \"%s\" (range %i to %i)\n",
					attributes[k].name, 
					attributes[k].min_value,
					attributes[k].max_value);

		    if(attributes[k].flags & XvSettable)
		    	fprintf(stdout, "              client settable attribute\n");

		    if(attributes[k].flags & XvGettable) {
			Atom the_atom;
			int value;

		    	fprintf(stdout, "              client gettable attribute");
			the_atom = XInternAtom(dpy, attributes[k].name, True);

			if(the_atom != None){
			   if((Success == XvGetPortAttribute(dpy, 
					ainfo[j].base_id, the_atom, &value)))
		    	       fprintf(stdout, " (current value is %i)", value);
			}
		        fprintf(stdout, "\n");
		    }

		}
		XFree(attributes);
	    } else {
		fprintf(stdout, "    no port attributes defined\n");
	    }

	    XvQueryEncodings(dpy, ainfo[j].base_id,
						&nencode, &encodings);

	    if(encodings && nencode) {
		int ImageEncodings = 0;

		for(n = 0; n < nencode; n++) {
		    if(!strcmp(encodings[n].name, "XV_IMAGE"))
			ImageEncodings++;
		}

		if(nencode - ImageEncodings) {
		    fprintf(stdout, "    number of encodings: %i\n", 
						nencode - ImageEncodings);

		    for(n = 0; n < nencode; n++) {
		    	if(strcmp(encodings[n].name, "XV_IMAGE")) {
			    fprintf(stdout, "      encoding ID #%li: \"%s\"\n",
					encodings[n].encoding_id,
					encodings[n].name);
			    fprintf(stdout, "        size: %li x %li\n",
					encodings[n].width,
					encodings[n].height);
			    fprintf(stdout, "        rate: %f\n",
					(float)encodings[n].rate.numerator/
					(float)encodings[n].rate.denominator);
			}
		    }
		}

		if(ImageEncodings && (ainfo[j].type & XvImageMask)) {
		   char imageName[5] = {0, 0, 0, 0, 0};

		   for(n = 0; n < nencode; n++) {
		      if(!strcmp(encodings[n].name, "XV_IMAGE")) {
			fprintf(stdout, 
				"    maximum XvImage size: %li x %li\n",	
				encodings[n].width, encodings[n].height);
			break;
		      }
		   }

                   formats = XvListImageFormats(
				dpy, ainfo[j].base_id, &numImages);

		   fprintf(stdout, "    Number of image formats: %i\n",
						numImages);

		   for(n = 0; n < numImages; n++) {
			memcpy(imageName, &(formats[n].id), 4);
			fprintf(stdout, "      id: 0x%x", formats[n].id);
			if(isprint(imageName[0]) && isprint(imageName[1]) &&
			  isprint(imageName[2]) && isprint(imageName[3])) 
			{
			  fprintf(stdout, " (%s)\n", imageName);
			} else {
			  fprintf(stdout, "\n");
			}
			fprintf(stdout, "        guid: ");
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[0]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[1]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[2]);
			fprintf(stdout, "%02x-", (unsigned char) 
						formats[n].guid[3]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[4]);
			fprintf(stdout, "%02x-", (unsigned char) 
						formats[n].guid[5]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[6]);
			fprintf(stdout, "%02x-", (unsigned char) 
						formats[n].guid[7]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[8]);
			fprintf(stdout, "%02x-", (unsigned char) 
						formats[n].guid[9]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[10]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[11]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[12]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[13]);
			fprintf(stdout, "%02x", (unsigned char) 
						formats[n].guid[14]);
			fprintf(stdout, "%02x\n", (unsigned char) 
						formats[n].guid[15]);

			fprintf(stdout, "        bits per pixel: %i\n",
					formats[n].bits_per_pixel);
			fprintf(stdout, "        number of planes: %i\n",
					formats[n].num_planes);
			fprintf(stdout, "        type: %s (%s)\n", 
			 (formats[n].type == XvRGB) ? "RGB" : "YUV",
			 (formats[n].format == XvPacked) ? "packed" : "planar");

			if(formats[n].type == XvRGB) {
			    fprintf(stdout, "        depth: %i\n", 
                                        formats[n].depth);

			    fprintf(stdout, "        red, green, blue masks: " 
					"0x%x, 0x%x, 0x%x\n", 
					formats[n].red_mask,
					formats[n].green_mask,
					formats[n].blue_mask);
			} else {

			}
			
	
		   }
		   if(formats) XFree(formats);
		}

		XvFreeEncodingInfo(encodings);
	    }
	    
	}

	XvFreeAdaptorInfo(ainfo);
    }
    return 1;
}
