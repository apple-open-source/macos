
                        * * *   FreeType/2   * * *

                      (Version 1.0, 5. November 1998)

 Copyright (C) 1997,1998 Michal Necasek <mike@mendelu.cz>
 Copyright (C) 1997,1998 The FreeType Development Team


 Motto: "OS/2 is dead? Again? Thanks for telling me, I'd never notice!"


  *** if you are upgrading from previous version, please see the FAQ (Q15) ***


- First a short Q&A:

Q1: What's this?
A1: This is what OS/2 users have been waiting for only too long - a free,
   high-quality TrueType renderer a.k.a. Font Driver conforming to the
   OS/2 Intelligent Font Interface specification. It is based on FreeType -
   a free portable library for using TrueType fonts.
    Please note that although this code is free the FreeType team and
   I will cheerfully accept any donations by happy users ;-) (not that I
   expect to get any)

Q2: How do I use this?
A2: Go to OS/2 command line and run INSTALL.CMD from the directory containing
   FREETYPE.DLL. This will replace the original IBM TrueType driver if it is
   installed.

Q3: Where's the disclaimer?
A3: No, don't worry, I didn't forget that. I of course provide NO WARRANTY
   that this code will work as you expect. Use only at your OWN RISK!

Q4: What should I do RIGHT NOW?
A4: Before attempting to install this driver, you are STRONGLY advised
   to archive your current configuration (Set Desktop Properties/Archive/
   Create archive at each startup, then reboot. Then of course switch archiving
   off). It is always possible your system won't boot with the font driver
   installed. You can risk it, but I warned you! You know how nasty the
   computers can be ;-)

Q5: What about the license?
A5: This code is distributed under the FreeType license.
    It is free and the source code is available as part of the FreeType
    distribution.

Q6: How do I get rid of this?
A6: Ah, right question. Just run UNINSTALL.CMD. That removes the font driver
    (not physically, it just isn't used on next startup) and restores the
    original TRUETYPE.DLL if it exists.

Q7: Is there something else?
A7: Yes, be prepared that the fonts just kick ass! You will no longer have
    to envy those poor souls still using the so-called 95% OS from THAT
    unspeakable company starting with the letter M ;-)


- Current features/bugs/limitations:

 Features   : - outlines
              - scaled/rotated text
              - supports printed output
              - works with TTCs (TrueType collections)
              - national characters (if provided in the font, of course);
                should work with all Latin codepages, Cyrillic and Greek.
              - partial DBCS support - Traditional Chinese should work about
                98%. Fonts like Times New Roman MT30 should work on all
                systems. If you want your language to be supported, you can
                apply at <mike@mendelu.cz> and become a Beta tester.

 Bug/feature: - unharmonious glyph spacing in some applications. This seems
                to come from OS/2's WYSIWYG glyph placement policy. This
                is more or less visible depending on the application. We
                can't do a lot about this... At least it's true WYSIWYG and
                no nasty surprises when printing.


 Limitations: - no grayscaling (a.k.a. antialiasing) - this is a limitation
                of OS/2, not my code. If OS/2 starts supporting it, I'll
                implement it the moment I lay my hands on the specs :)
                Unfortunately it most probably won't happen any too soon.
                Anyway, you have to bug IBM about this one, not me!

- Planned features and features under consideration:
   - possibly adding even support for Type 1 fonts, but that depends on
     further FreeType engine development. Looks quite probable now.

And finally, thanks go to:
 - the FreeType team, the makers of FreeType. Without them, my work would be
   impossible.
 - especially David Turner of FreeType for his help, advice and support
 - Robert Muchsel, I used one or two ideas from his code
 - Marc L Cohen, Ken Borgendale and Tetsuro Nishimura from IBM. They provided
   me with lots of extremely valuable 'inside' information.
 - and last but not least - IBM, for providing us with the wonderful OS/2.
   And for giving out the necessary docs for free. If all companies did
   that, the world would be a better place.


Information on FreeType is available at
 http://www.freetype.org

Please send bug reports/suggestions and comments to :

   freetype-os2@physiol.med.tu-muenchen.de


Greetings can also be sent directly to the author at : mike@mendelu.cz

And if you didn't know, IBM and OS/2 are registered trademarks of the
International Business Machines Corporation.
