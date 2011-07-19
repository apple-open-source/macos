# ----------------------------------------------------------------------------
#  tmpldlg.tcl
#  This file is part of Unifix BWidget Toolkit
#  $Id: tmpldlg.tcl,v 1.5 2009/09/08 21:21:43 oberdorfer Exp $
# ----------------------------------------------------------------------------
#

namespace eval DemoDlg {
    variable tmpl
    variable msg
    variable progmsg
    variable progval
    variable resources "en"
}


proc DemoDlg::create { nb } {
    set frame [$nb insert end demoDlg -text "Dialog"]

    set titf1 [TitleFrame $frame.titf1 -text "Resources"]
    set titf2 [TitleFrame $frame.titf2 -text "Template Dialog"]
    set titf3 [TitleFrame $frame.titf3 -text "Message Dialog"]
    set titf4 [TitleFrame $frame.titf4 -text "Other dialog"]

    set subf [$titf1 getframe]
    set cmd  {option read [file join $::BWIDGET::LIBRARY "lang" $DemoDlg::resources.rc]}
    set rad1 [BWidget::wrap radiobutton $subf.rad1 -text "English" \
                  -variable DemoDlg::resources -value en \
                  -command  $cmd]
    set rad2 [BWidget::wrap radiobutton $subf.rad2 -text "French" \
                  -variable DemoDlg::resources -value fr \
                  -command  $cmd]
    set rad3 [BWidget::wrap radiobutton $subf.rad3 -text "German" \
                  -variable DemoDlg::resources -value de \
                  -command  $cmd]
    pack $rad1 $rad2 $rad3 -side left -padx 5

    _tmpldlg [$titf2 getframe]
    _msgdlg  [$titf3 getframe]
    _stddlg  [$titf4 getframe]

    pack $titf1 -fill x -pady 2 -padx 2
    pack $titf4 -side bottom -fill x -pady 2 -padx 2
    pack $titf2 $titf3 -side left -padx 2 -fill both -expand yes
}


proc DemoDlg::_tmpldlg { parent } {
    variable tmpl

    set tmpl(side)   bottom
    set tmpl(anchor) c

    set labf1 [LabelFrame $parent.labf1 -text "Button side" -side top \
                   -anchor w -relief sunken -borderwidth 1]
    set subf  [$labf1 getframe]
    BWidget::wrap radiobutton $subf.rad1 -text "Bottom" \
        -variable DemoDlg::tmpl(side) -value bottom
    BWidget::wrap radiobutton $subf.rad2 -text "Left" \
        -variable DemoDlg::tmpl(side) -value left
    BWidget::wrap radiobutton $subf.rad3 -text "Right" \
        -variable DemoDlg::tmpl(side) -value right
    BWidget::wrap radiobutton $subf.rad4 -text "Top" \
        -variable DemoDlg::tmpl(side) -value top

    pack $subf.rad1 $subf.rad2 $subf.rad3 $subf.rad4 -anchor w -padx 5

    set labf2 [LabelFrame $parent.labf2 -text "Button anchor" -side top \
                   -anchor w -relief sunken -borderwidth 1]
    set subf  [$labf2 getframe]
    BWidget::wrap radiobutton $subf.rad1 -text "North" \
        -variable DemoDlg::tmpl(anchor) -value n
    BWidget::wrap radiobutton $subf.rad2 -text "West" \
        -variable DemoDlg::tmpl(anchor) -value w
    BWidget::wrap radiobutton $subf.rad3 -text "East" \
        -variable DemoDlg::tmpl(anchor) -value e
    BWidget::wrap radiobutton $subf.rad4 -text "South" \
        -variable DemoDlg::tmpl(anchor) -value s
    BWidget::wrap radiobutton $subf.rad5 -text "Center" \
        -variable DemoDlg::tmpl(anchor) -value c

    pack $subf.rad1 $subf.rad2 $subf.rad3 $subf.rad4 $subf.rad5 -anchor w -padx 5

    set sep    [Separator  $parent.sep -orient horizontal]
    set button [Button $parent.but -text "Show" \
                                 -command DemoDlg::_show_tmpldlg]

    pack $button -side bottom
    pack $sep -side bottom -fill x -pady 10
    pack $labf1 $labf2 -side left -padx 4 -anchor n
}


proc DemoDlg::_msgdlg { parent } {
    variable msg

    set msg(type) ok
    set msg(icon) info

    set labf1 [LabelFrame $parent.labf1 -text "Type" -side top \
                   -anchor w -relief sunken -borderwidth 1]
    set subf  [$labf1 getframe]
    BWidget::wrap radiobutton $subf.rad1 -text "Ok" -variable DemoDlg::msg(type) -value ok
    BWidget::wrap radiobutton $subf.rad2 -text "Ok, Cancel" -variable DemoDlg::msg(type) -value okcancel
    BWidget::wrap radiobutton $subf.rad3 -text "Retry, Cancel" -variable DemoDlg::msg(type) -value retrycancel
    BWidget::wrap radiobutton $subf.rad4 -text "Yes, No" -variable DemoDlg::msg(type) -value yesno
    BWidget::wrap radiobutton $subf.rad5 -text "Yes, No, Cancel" -variable DemoDlg::msg(type) -value yesnocancel
    BWidget::wrap radiobutton $subf.rad6 -text "Abort, Retry, Ignore" -variable DemoDlg::msg(type) -value abortretryignore
    BWidget::wrap radiobutton $subf.rad7 -text "User" -variable DemoDlg::msg(type) -value user
    Entry $subf.user -textvariable DemoDlg::msg(buttons)

    pack $subf.rad1 $subf.rad2 $subf.rad3 $subf.rad4 $subf.rad5 $subf.rad6 -anchor w -padx 5
    pack $subf.rad7 $subf.user -side left -padx 5

    set labf2 [LabelFrame $parent.labf2 -text "Icon" -side top -anchor w -relief sunken -borderwidth 1]
    set subf  [$labf2 getframe]
    BWidget::wrap radiobutton $subf.rad1 -text "Information" -variable DemoDlg::msg(icon) -value info
    BWidget::wrap radiobutton $subf.rad2 -text "Question"    -variable DemoDlg::msg(icon) -value question
    BWidget::wrap radiobutton $subf.rad3 -text "Warning"     -variable DemoDlg::msg(icon) -value warning
    BWidget::wrap radiobutton $subf.rad4 -text "Error"       -variable DemoDlg::msg(icon) -value error
    pack $subf.rad1 $subf.rad2 $subf.rad3 $subf.rad4 -anchor w -padx 5


    set sep    [Separator  $parent.sep -orient horizontal]
    set button [BWidget::wrap button $parent.but -text "Show" -command DemoDlg::_show_msgdlg]

    pack $button -side bottom
    pack $sep -side bottom -fill x -pady 10
    pack $labf1 $labf2 -side left -padx 4 -anchor n
}


proc DemoDlg::_stddlg { parent } {
  
  
  set frm [BWidget::wrap frame $parent.f0]
    pack $frm -side top -fill x
  
    set but0  [Button $frm.but0 \
                   -text "Select a color - popup" \
                   -command "DemoDlg::_show_color $parent popup"]

    set but1  [Button $frm.but1 \
                   -text "Select a color - Dialog" \
                   -command "DemoDlg::_show_color $parent dialog"]

    set but2  [Button $frm.but2 \
                   -text    "Font selector dialog" \
                   -command DemoDlg::_show_fontdlg]

    set but3  [Button $frm.but3 \
                   -text    "Progression dialog" \
                   -command DemoDlg::_show_progdlg]

    set but4  [Button $frm.but4 \
                   -text    "Password dialog" \
                   -command DemoDlg::_show_passdlg]

    pack $but0 $but1 $but2 $but3 $but4 -side left -padx 5 -anchor w -padx 5
}


proc DemoDlg::_show_color {w {mode "popup"}} {

    if { [BWidget::using ttk] } {
             set ccolor $::BWidget::colors(SystemButtonFace)
    } else { set ccolor [$w cget -background] }


    if { $mode == "popup" } {
        set color [SelectColor::menu $w.color \
	             [list below $w] -color $ccolor]
    } else {
        set color [SelectColor::dialog $w.color \
	             -parent [list below $w] -title "Select Color Dialog"]
    }

    if { [string length $color] > 0 } {

        if { [BWidget::using ttk] } {
	
	    ::ttk::style configure . -background $color

	} else {
            [winfo parent $w] configure -background $color
      }
    }
}

proc DemoDlg::_show_tmpldlg { } {
    variable tmpl

    set dlg [Dialog .tmpldlg -parent . -modal local \
                 -separator 1 \
                 -title   "Template dialog" \
                 -side    $tmpl(side)    \
                 -anchor  $tmpl(anchor)  \
                 -default 0 -cancel 1]
    $dlg add -name ok
    $dlg add -name cancel
    set msg [message [$dlg getframe].msg -text "Template\nDialog" -justify center -anchor c]
    pack $msg -fill both -expand yes -padx 100 -pady 100
    $dlg draw
    destroy $dlg
}


proc DemoDlg::_show_msgdlg { } {
    variable msg

    destroy .msgdlg
    MessageDlg .msgdlg -parent . \
        -message "Message for MessageBox" \
        -type    $msg(type) \
        -icon    $msg(icon) \
        -buttons $msg(buttons)
}


proc DemoDlg::_show_fontdlg { } {
    # -initialcolor Black
    set font [SelectFont .fontdlg -parent . -font $Demo::font]

    if { $font != "" } {
        Demo::update_font $font
    }
}


proc DemoDlg::_show_progdlg { } {
    set DemoDlg::progmsg "Compute in progress..."
    set DemoDlg::progval 0

    ProgressDlg .progress -parent . -title "Wait..." \
        -type         infinite \
        -width        20 \
        -textvariable DemoDlg::progmsg \
        -variable     DemoDlg::progval \
        -stop         "Stop" \
        -command      {destroy .progress}
    _update_progdlg
}


proc DemoDlg::_update_progdlg { } {
    if { [winfo exists .progress] } {
        set DemoDlg::progval 2
        after 20 DemoDlg::_update_progdlg
    }
}

proc DemoDlg::_show_passdlg { } {
    PasswdDlg .passwd -parent .
}

