//#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>



/*************** Micro getopt() *********************************************/
#define	OPTION(c,v)	(_O&2&&**v?*(*v)++:!c||_O&4?0:(!(_O&1)&& \
                                (--c,++v),_O=4,c&&**v=='-'&&v[0][1]?*++*v=='-'\
                                &&!v[0][1]?(--c,++v,0):(_O=2,*(*v)++):0))
#define	OPTARG(c,v)	(_O&2?**v||(++v,--c)?(_O=1,--c,*v++): \
                                (_O=4,(char*)0):(char*)0)
#define	OPTONLYARG(c,v)	(_O&2&&**v?(_O=1,--c,*v++):(char*)0)
#define	ARG(c,v)	(c?(--c,*v++):(char*)0)

static int _O = 0;		/* Internal state */
/*************** Micro getopt() *********************************************/

u_long ppplink = 0xFFFF;

int main(int argc, const char *argv[]) {

    int option;
    char *arg;

    while ((option = OPTION(argc, argv)) != 0) {
        switch (option) {
        case 'l':
            if ((arg = OPTARG(argc, argv)) != NULL)
                ppplink = atoi(arg);

            break;
        }
    }

    return NSApplicationMain(argc, argv);
}

