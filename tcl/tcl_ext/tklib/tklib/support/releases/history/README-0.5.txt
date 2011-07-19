Overview
========

         7 new packages         in  6 new modules.
         6 new packages         in  1 existing module.
        17 changed packages     in 11 modules.

New in Tklib 0.5
================

Module          Package                 New Version     Comments
------          -------                 -----------     -----------------------
canvas          canvas::sqmap           0.2             Canvas with square-tiled background
                canvas::zoom            0.1             Primitive zoom control/megawidget
------          -------                 -----------     -----------------------
chatwidget      chatwidget              1.1.0           Megawidget for chat applications
crosshair       crosshair               1.0.2           Canvas crosshairs
diagrams        Diagrams                0.2             Drawing diagrams (similar to pic nroff)
khim            khim                    1.0.1           Kevin's Hacky Input Method
ntext           ntext                   0.81            Alternative Bindings for the Text Widget
------          -------                 -----------     -----------------------
widget          widget::calendar        0.9             Snidgets, calendar,
                widget::dateentry       0.91            Date entry field
                widget::menuentry       1.0             Menu entry
                widget::statusbar       1.2             Statusbar
                widget::scrolledtext    1.0             Scrolled text display
                widget::toolbar         1.2             Toolbar container
------          -------                 -----------     -----------------------

Changes from Tklib 0.4.1 to 0.5
===============================

                                Tklib 0.4.1     Tklib 0.5
Module          Package         Old version     New Version     Comments
------          -------         -----------     -----------     ---------------
ctext           ctext           3.1             3.2             B, D, EF        
cursor          cursor          0.1             0.2             EF              
datefield       datefield       0.1             0.2             EF
------          -------         -----------     -----------     ---------------
ico             ico             0.3             1.0.3           B, D, API
                                                0.3.1           B, D
------          -------         -----------     -----------     ---------------
ipentry         ipentry         0.1             0.3             EF, B
plotchart       plotchart       1.1             1.6.1           EF, D, EX
style           style::as       1.3             1.4             EF, B
swaplist        swaplist        0.1             0.2             B, D
tablelist       tablelist       4.2             4.10.1          D, EX, B, EF
------          -------         -----------     -----------     ---------------
tooltip         tooltip         1.1             1.4.4           B, D, EF
                tipstack        1.0             1.0.1           B
------          -------         -----------     -----------     ---------------
widget          widget::all             1.0     1.2
                widget::screenruler     1.1     1.2             B
                widget::ruler           1.0     1.1             B
                widget::scrolledwindow  1.0     1.2             B
                widget::dialog          1.2     1.3             EF, B
                widget::panelframe      1.0     1.1             EF
------          -------         -----------     -----------     ---------------

Invisible changes (no version change)
------          -------         -----------     -----------     ---------------
autoscroll      autoscroll                      1.1             D
tkpiechart      tkpiechart                      6.6             EX, load changes
------          -------         -----------     -----------     ---------------

Legend  Change  Details Comments
        ------  ------- ---------
        Major   API:    ** incompatible ** API changes.

        Minor   EF :    Extended functionality, API.
                I  :    Major rewrite, but no API change

        Patch   B  :    Bug fixes.
                EX :    New examples.
                P  :    Performance enhancement.

        None    T  :    Testsuite changes.
                D  :    Documentation updates.
