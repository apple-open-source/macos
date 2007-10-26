// { dg-do assemble  }
// { dg-options "-Wshadow" }
// GROUPS passed shadow-warnings
// shadow file
// Message-Id: <9211061827.AA03517@harvey>
// From: Jeff Gehlhaar <jbg@qualcomm.com>
// Subject: GCC Bug..
// Date: Fri, 6 Nov 1992 10:27:10 -0700

// APPLE LOCAL mainline 2006-10-13 3904173
class Klasse
{
public:
        // APPLE LOCAL mainline 2006-10-13 3904173
        Klasse(void);           // constructor
        int Shadow(void);       // member function
private:
        long value;
};

// APPLE LOCAL mainline 2006-10-13 3904173
Klasse::Klasse(void)
{
        value = 0;
}

static inline unsigned char
Function(int Shadow)
{
        return 0;
}
