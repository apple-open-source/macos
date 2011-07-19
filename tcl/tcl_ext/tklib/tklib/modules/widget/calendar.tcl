# -*- tcl -*-
#
# calendar.tcl -
#
#	Calendar widget drawn on a canvas.
#	Adapted from Suchenwirth code on the wiki.
#
# Copyright (c) 2008 Rüdiger Härtel
#
# RCS: @(#) $Id: calendar.tcl,v 1.9 2010/07/16 00:19:57 hobbs Exp $
#

#
# Creation and Options - widget::calendar $path ...
# -command        -default {}
# -dateformat     -default "%m/%d/%Y"
# -font           -default {Helvetica 9}
# -textvariable   -default {}
# -firstday       -default "monday"
# -highlightcolor -default "#FFCC00"
# -shadecolor     -default "#888888"
# -language       -default en   Supported languages: de, en, es, fr, gr,
#                                he, it, ja, sv, pt, zh, fi ,tr, nl, ru,
#                                crk, crx-nak, crx-lhe
#
#  All other options to canvas
#
# Methods
#  $path get <part>   => selected date, part can be
#                              day,month,year, all
#                         default is all
#  All other methods to canvas
#
# Bindings
#  NONE
#

if 0 {
    # Samples
    package require widget::calendar
    #set db [widget::calendar .db]
    #pack $sw -fill both -expand 1
}

###

package require widget

snit::widgetadaptor widget::calendar {
    delegate option * to hull
    delegate method * to hull

    option -firstday       -default monday        -configuremethod C-refresh \
					      -type [list snit::enum -values [list sunday monday]]
    option -textvariable   -default {}            -configuremethod C-textvariable

    option -command        -default {}
    option -dateformat     -default "%m/%d/%Y"    -configuremethod C-refresh
    option -font           -default {Helvetica 9} -configuremethod C-font
    option -highlightcolor -default "#FFCC00"     -configuremethod C-refresh
    option -shadecolor     -default "#888888"     -configuremethod C-refresh
    option -language       -default en            -configuremethod C-language
    option -showpast       -default 1             -configuremethod C-refresh \
						  -type {snit::boolean} 


    variable fullrefresh 1
    variable pending "" ; # pending after id for refresh
    variable data -array {
	day 01 month 01 year 2007
	linespace 0 cellspace 0
	selday {} selmonth {} selyear {}
    }

    constructor args {
	installhull using canvas -highlightthickness 0 -borderwidth 0 \
	    -background white
	bindtags $win [linsert [bindtags $win] 1 Calendar]

	set now [clock scan "today 00:00:00"]

	foreach {data(day) data(month) data(year)} \
	    [clock format $now -format "%e %m %Y"] { break }
	scan $data(month) %d data(month) ; # avoid leading 0 issues

	set data(selday)   $data(day)
	set data(selmonth) $data(month)
	set data(selyear)  $data(year)

	# Binding for the 'day' tagged items
	$win bind day <1>           [mymethod invoke]

	    # move days
	bind $win <Left>            [mymethod adjust -1  0  0]
	bind $win <Right>           [mymethod adjust  1  0  0]
	    # move weeks
	bind $win <Up>              [mymethod adjust -7  0  0]
	bind $win <Down>            [mymethod adjust  7  0  0]
	    # move months
	bind $win <Control-Left>    [mymethod adjust  0 -1  0]
	bind $win <Control-Right>   [mymethod adjust  0  1  0]
	    # move years
	bind $win <Control-Up>      [mymethod adjust  0  0 -1]
	bind $win <Control-Down>    [mymethod adjust  0  0  1]

	$self configurelist $args

	$self reconfigure
	$self refresh
    }

    destructor {  
	if { $options(-textvariable) ne "" } {
	    trace remove variable $options(-textvariable) write [mymethod DoUpdate]
	}
    }

    #
    # C-font --
    #
    #  Configure the font of the widget
    #
    ##
    method C-font {option value} {
	set options($option) $value
	$self reconfigure
	set fullrefresh 1
	$self refresh
    }

    #
    # C-refresh --
    #
    #  Place holder for all options that need a refresh after
    #  takeing over the new option.
    #
    ##
    method C-refresh {option value} {
	set options($option) $value
	$self refresh
    }

    #
    # C-textvariable --
    #
    #  Configure the textvariable for the widget. Installs a
    #  trace handler for the variable.
    #  If an empty textvariable is given the trace handler is
    #  uninstalled.
    #
    ##
    method C-textvariable {option value} {

        if { [string match "::widget::dateentry::Snit*" $value] } {
            return
        }

	if {![string match ::* $value]} {
	    set value ::$value
	}
	set options($option) $value

	if {$value ne "" } {
	    trace remove variable $options(-textvariable) write [mymethod DoUpdate]

	    if { ![info exists $options($option)] } {
	        set now [clock seconds]
	        set $options($option) [clock format $now -format $options(-dateformat)]
	    }

	    trace add variable ::$value write [mymethod DoUpdate]
	    if { [info exists $value] } {
		$self DoUpdate
	    }
	}
    }

    #
    # C-language --
    #
    #  Configure the language of the calendar.
    #
    ##
    method C-language {option value} {

	set langs [list \
		    de en es fr gr he it ja sv pt zh fi tr nl ru \
		    crk  \
		    crx-nak \
		    crx-lhe \
	]
	if { $value ni $langs } {
	    return -code error "Unsupported language. Choose one of: $langs"
	}

	set options($option) $value

	$self refresh
    }

    #
    # DoUpdate --
    #
    #  Update the internal values of day, month and year when the
    #  textvariable is written to (trace callback).
    #
    ##
    method DoUpdate { args } {

	set value $options(-textvariable)
	set tmp [set $value]
	if {$tmp eq ""} { return }
	if {$::tcl_version < 8.5} {
	    # Prior to 8.4, users must use [clock]-recognized dateformat
	    set date [clock scan $tmp]
	} else {
	    set date [clock scan $tmp -format $options(-dateformat)]
	}

	foreach {data(day) data(month) data(year)} \
	    [clock format $date -format "%e %m %Y"] { break }
	scan $data(month) %d data(month) ; # avoid leading 0 issues

	set data(selday)   $data(day)
	set data(selmonth) $data(month)
	set data(selyear)  $data(year)

	$self refresh
    }

    #
    # get --
    #   Return parts of the selected date or the complete date.
    #
    # Arguments:
    #   what  - Selects the part of the date or the complete date.
    #            values <day,month,year, all>, default is all
    #
    ##
    method get {{what all}} {
	switch -exact -- $what {
	    "day"   { return $data(selday) }
	    "month" { return $data(selmonth) }
	    "year"  { return $data(selyear) }
	    "all"   {
		if {$data(selday) ne ""} {
		    set date [clock scan $data(selmonth)/$data(selday)/$data(selyear)]
		    set fmtdate [clock format $date -format $options(-dateformat)]
		    return $fmtdate
		}
	    }
	    default {
		return -code error "unknown component to retrieve \"$what\",\
			must be one of day, month or year"
	    }
	}
    }

    #
    # adjust --
    #
    #   Adjust internal values of the calendar and update the contents
    #   of the widget. This function is invoked by pressing the arrows
    #   in the widget and on key bindings.
    #
    # Arguments:
    #   dday    - Difference in days
    #   dmonth  - Difference in months
    #   dyear   - Difference in years
    #
    ##
    method adjust {dday dmonth dyear} {
	incr data(year)  $dyear
	incr data(month) $dmonth

	set maxday [$self numberofdays $data(month) $data(year)]

	if { ($data(day) + $dday) < 1}  {
	    incr data(month) -1

	    set maxday [$self numberofdays $data(month) $data(year)]
	    set  data(day) [expr {($data(day) + $dday) % $maxday}]

	} else {

	    if { ($data(day) + $dday) > $maxday } {

		incr data(month) 1
		set  data(day)   [expr {($data(day) + $dday) % $maxday}]

	    } else {
		incr data(day) $dday
	    }
	}
	    

	if { $data(month) > 12} {
	    set  data(month) 1
	    incr data(year)
	}

	if { $data(month) < 1}  {
	    set  data(month) 12
	    incr data(year)  -1
	}


	set maxday [$self numberofdays $data(month) $data(year)]
	if { $maxday < $data(day) } {
	    set data(day) $maxday
	}
	set data(selday)   $data(day)
	set data(selmonth) $data(month)
	set data(selyear)  $data(year)

	$self refresh
    }

    method cbutton {x y w command} {
	# Draw simple arrowbutton using Tk's line arrows
	set wd [expr {abs($w)}]
	set wd2 [expr {$wd/2. - ((abs($w) < 10) ? 1 : 2)}]
	set poly [$hull create line $x $y [expr {$x+$w}] $y -arrow last \
		      -arrowshape [list $wd $wd $wd2] \
		      -tags [list cbutton shadetext]]
	$hull bind $poly <1> $command
    }

    method reconfigure {} {
	set data(cellspace) [expr {[font measure $options(-font) "30"] * 2}]
	set w [expr {$data(cellspace) * 8}]
	set data(linespace) [font metrics $options(-font) -linespace]
	set h [expr {int($data(linespace) * 9.25)}]
	$hull configure -width $w -height $h
    }

    method refresh { } {
	# Idle deferred refresh
	after cancel $pending
	set pending [after idle [mymethod Refresh ]]
    }

    method Refresh { } {
	# Set up coords based on font spacing
	set x  [expr {$data(cellspace) / 2}]; set x0 $x
	set dx $data(cellspace)

	set y [expr {int($data(linespace) * 1.75)}]
	set dy $data(linespace)
	set pad [expr {$data(linespace) / 2}]

	set xmax [expr {$x0+$dx*6}]
	set winw [$hull cget -width]
	set winh [$hull cget -height]

	if {$fullrefresh} {
	    set fullrefresh 0
	    $hull delete all

	    # Left and Right buttons
	    set xs [expr {$data(cellspace) / 2}]
	    $self cbutton [expr {$xs+2}] $pad -$xs              [mymethod adjust 0  0 -1]; # <<
	    $self cbutton [expr {$xs*2}] $pad [expr {-$xs/1.5}] [mymethod adjust 0 -1  0]; # <
	    set lxs [expr {$winw - $xs - 2}]
	    $self cbutton $lxs $pad $xs                         [mymethod adjust 0  0  1]; # >>
	    incr lxs -$xs
	    $self cbutton $lxs $pad [expr {$xs/1.5}]            [mymethod adjust 0  1  0]; # >

	    # day (row) and weeknum (col) headers
	    $hull create rect 0 [expr {$y - $pad}] $winw [expr {$y + $pad}] \
		-tags shade
	    $hull create rect 0 [expr {$y - $pad}] $dx $winh -tags shade
	} else {
	    foreach tag {title otherday day highlight week} {
		$hull delete $tag
	    }
	}

	# Title "Month Year"
	set title [$self formatMY $data(month) $data(year)]
	$hull create text [expr {$winw/2}] $pad -text $title -tag title \
	    -font $options(-font) -fill blue

	# weekdays - could be drawn on fullrefresh, watch -firstday change
	set weekdays $LANGS(weekdays,$options(-language))
	if {$options(-firstday) eq "monday"} { $self lcycle weekdays }
	foreach i $weekdays {
	    incr x $dx
	    $hull create text $x $y -text $i -fill white \
		-font $options(-font) -tag title
	}

	# place out the days
	set first $data(month)/1/$data(year)
	set weekday [clock format [clock scan $first] -format %w]
	if {$options(-firstday) eq "monday"} {
	    set weekday [expr {($weekday+6)%7}]
	}

	# Print days preceding the 1st of the month
	set x [expr {$x0+$weekday*$dx}]
	set x1 $x; set offset 0
	incr y $dy
	while {$weekday} {
	    set t [clock scan "$first [incr offset] days ago"]
	    set day [clock format $t -format "%e"] ; # %d w/o leading 0
	    $hull create text $x1 $y -text $day \
		-font $options(-font) -tags [list otherday shadetext]
	    incr weekday -1
	    incr x1 -$dx
	}
	set dmax [$self numberofdays $data(month) $data(year)]

	for {set d 1} {$d <= $dmax} {incr d} {
	    incr x $dx
	    if {($options(-showpast) == 0)
		&& ($d < $data(selday))
		&& ($data(month) <= $data(selmonth))
		&& ($data(year) <= $data(selyear))} {
		# XXX day in the past - above condition currently broken
		set id [$hull create text $x $y -text $d \
			    -tags [list otherday shadetext] \
			    -font $options(-font)]
	    } else {
		# current month day
		set id [$hull create text $x $y -text $d -tag day \
			    -font $options(-font)]
	    }
	    if {$d == $data(selday) && ($data(month) == $data(selmonth))} {
		# selected day
		$hull create rect [$hull bbox $id] -tags [list day highlight]
	    }
	    $hull raise $id
	    if {$x > $xmax} {
		# Week of the year
		set x $x0
		set week [$self getweek $d $data(month) $data(year)]
		$hull create text [expr {$x0}] $y -text $week -tag week \
		    -font $options(-font) -fill white
		incr y $dy
	    }
	}
	# Week of year (last day)
	if {$x != $x0} {
	    set week [$self getweek $dmax $data(month) $data(year)]
	    $hull create text [expr {$x0}] $y -text $week -tag week \
		-font $options(-font) -fill white
	    for {set d 1} {$x <= $xmax} {incr d} {
		incr x $dx
		$hull create text $x $y -text $d \
		    -tags [list otherday shadetext] \
		    -font $options(-font)
	    }
	}

	# Display Today line
	set now [clock seconds]
	set today "$LANGS(today,$options(-language)) [clock format $now -format $options(-dateformat)]"
	$hull create text [expr {$winw/2}] [expr {$winh - $pad}] -text $today \
	    -tag week -font $options(-font) -fill black

	# Make sure options-based items are set
	$hull itemconfigure highlight \
	    -fill $options(-highlightcolor) \
	    -outline $options(-highlightcolor)
	$hull itemconfigure shadetext -fill $options(-shadecolor)
	$hull itemconfigure shade -fill $options(-shadecolor) \
	    -outline $options(-shadecolor)
    }

    method getweek {day month year} {
	set _date [clock scan $month/$day/$year]
	return [clock format $_date -format %V]
    }

    method invoke {} {

	catch {focus -force $win} msg
	if { $msg ne "" } {
	#    puts $msg
	}
	set item [$hull find withtag current]
	set data(day) [$hull itemcget $item -text]

	set data(selday) $data(day)
	set data(selmonth) $data(month)
	set data(selyear) $data(year)
	set date    [clock scan   $data(month)/$data(day)/$data(year)]
	set fmtdate [clock format $date -format $options(-dateformat)]

	if {$options(-textvariable) ne {}} {
	    set $options(-textvariable) $fmtdate
	}

	if {$options(-command) ne {}} {
	    # pass single arg of formatted date chosen
	    uplevel \#0 $options(-command) [list $fmtdate]
	}

	$self refresh
    }

    method formatMY {month year} {
	set lang $options(-language)
	if {[info exists LANGS(mn,$lang)]} {
	    set month [lindex $LANGS(mn,$lang) $month]
	} else {
	    set _date [clock scan $month/1/$year]
	    set month [clock format $_date -format %B] ; # full month name
	}
	if {[info exists LANGS(format,$lang)]} {
	    set format $LANGS(format,$lang)
	} else {
	    set format "%m %Y" ;# default
	}
	# Replace month/year and do any necessary substs
	return [subst [string map [list %m $month %Y $year] $format]]
    }

    method numberofdays {month year} {
	if {$month == 12} {set month 0; incr year}
	clock format [clock scan "[incr month]/1/$year	1 day ago"] -format %d
    }

    method lcycle _list {
	upvar $_list list
	set list [concat [lrange $list 1 end] [list [lindex $list 0]]]
    }

    typevariable LANGS -array {
	mn,crk {
	    . Kis\u01E3p\u012Bsim Mikisiwip\u012Bsim Niskip\u012Bsim Ay\u012Bkip\u012Bsim
	    S\u0101kipak\u0101wip\u012Bsim
	    P\u0101sk\u0101wihowip\u012Bsim Paskowip\u012Bsim Ohpahowip\u012Bsim
	    N\u014Dcihitowip\u012Bsim Pin\u0101skowip\u012Bsim Ihkopiwip\u012Bsim
	    Paw\u0101cakinas\u012Bsip\u012Bsim
	}
	weekdays,crk {P\u01E3 N\u01E3s Nis N\u01E3 Niy Nik Ay}
	today,crk {}

	mn,crx-nak {
	    . {Sacho Ooza'} {Chuzsul Ooza'} {Chuzcho Ooza'} {Shin Ooza'} {Dugoos Ooza'} {Dang Ooza'}\
		{Talo Ooza'} {Gesul Ooza'} {Bit Ooza'} {Lhoh Ooza'} {Banghan Nuts'ukih} {Sacho Din'ai}
	}
	weekdays,crx-nak {Ji Jh WN WT WD Ts Sa}
	today,crx-nak {}

	mn,crx-lhe {
	    . {'Elhdzichonun} {Yussulnun} {Datsannadulhnun} {Dulats'eknun} {Dugoosnun} {Daingnun}\
		{Gesnun} {Nadlehcho} {Nadlehyaz} {Lhewhnandelnun} {Benats'ukuihnun} {'Elhdziyaznun}
	}
	weekdays,crx-lhe {Ji Jh WN WT WD Ts Sa}
	today,crx-lhe {}

	mn,de {
	    . Januar Februar März April Mai Juni Juli August
	    September Oktober November Dezember
	}
	weekdays,de {So Mo Di Mi Do Fr Sa}
	today,de {Heute ist der}

	mn,en {
	    . January February March April May June July August
	    September October November December
	}
	weekdays,en {Su Mo Tu We Th Fr Sa}
	today,en {Today is}

	mn,es {
	    . Enero Febrero Marzo Abril Mayo Junio Julio Agosto
	    Septiembre Octubre Noviembre Diciembre
	}
	weekdays,es {Do Lu Ma Mi Ju Vi Sa}
	today,es {}

	mn,fr {
	    . Janvier Février Mars Avril Mai Juin Juillet Août
	    Septembre Octobre Novembre Décembre
	}
	weekdays,fr {Di Lu Ma Me Je Ve Sa}
	today,fr {}

	mn,gr {
	    . Îýý???Ïýý?Ïýý??Ïýý ???Ïýý?Ïýý?Ïýý??Ïýý Îýý?ÏýýÏýý??Ïýý ÎýýÏýýÏýý????Ïýý Îýý?Îýý?Ïýý Îýý?Ïýý???Ïýý Îýý?Ïýý???Ïýý ÎýýÏýý??ÏýýÏýýÏýý?Ïýý
	    ??ÏýýÏýýÎýý??Ïýý??Ïýý Îýý?ÏýýÏýý??Ïýý??Ïýý Îýý?Îýý??Ïýý??Ïýý Îýý??Îýý??Ïýý??Ïýý
	}
	weekdays,gr {ÎýýÏýýÏýý Îýý?Ïýý TÏýý? ??Ïýý Î ?? Î ?Ïýý ???}
	today,gr {}

	mn,he {
	    . ×ýý× ×ýý×ýý? ?×ýý?×ýý×ýý? ×ýý?? ×ýý??×ýý×ýý ×ýý×ýý×ýý ×ýý×ýý× ×ýý ×ýý×ýý×ýý×ýý ×ýý×ýý×ýý×ýý?×ýý ??×ýý×ýý×ýý? ×ýý×ýý?×ýý×ýý×ýý? × ×ýý×ýý×ýý×ýý? ×ýý?×ýý×ýý?
	}
	weekdays,he {?×ýý?×ýý×ýý ?× ×ýý ?×ýý×ýý?×ýý ?×ýý×ýý?×ýý ×ýý×ýý×ýý?×ýý ?×ýý?×ýý ?×ýý?}
	today,he {}

	mn,it {
	    . Gennaio Febraio Marte Aprile Maggio Giugno Luglio Agosto
	    Settembre Ottobre Novembre Dicembre
	}
	weekdays,it {Do Lu Ma Me Gi Ve Sa}
	today,it {}

	format,ja {%Y\u5e74 %m\u6708}
	weekdays,ja {\u65e5 \u6708 \u706b \u6c34 \u6728 \u91d1 \u571f}
	today,ja {}

	mn,nl {
	    . januari februari maart april mei juni juli augustus
	    september oktober november december
	}
	weekdays,nl {Zo Ma Di Wo Do Vr Za}
	today,nl {}

	mn,ru {
	    . \u042F\u043D\u0432\u0430\u0440\u044C
	    \u0424\u0435\u0432\u0440\u0430\u043B\u044C \u041C\u0430\u0440\u0442
	    \u0410\u043F\u0440\u0435\u043B\u044C \u041C\u0430\u0439
	    \u0418\u044E\u043D\u044C \u0418\u044E\u043B\u044C
	    \u0410\u0432\u0433\u0443\u0441\u0442
	    \u0421\u0435\u043D\u0442\u044F\u0431\u0440\u044C
	    \u041E\u043A\u0442\u044F\u0431\u0440\u044C \u041D\u043E\u044F\u0431\u0440\u044C
	    \u0414\u0435\u043A\u0430\u0431\u0440\u044C
	}
	weekdays,ru {
	    \u432\u43e\u441 \u43f\u43e\u43d \u432\u442\u43e \u441\u440\u435
	    \u447\u435\u442 \u43f\u44f\u442 \u441\u443\u431
	}
	today,ru {}

	mn,sv {
	    . januari februari mars april maj juni juli augusti
	    september oktober november december
	}
	weekdays,sv {s\u00F6n m\u00E5n tis ons tor fre l\u00F6r}
	today,sv {}

	mn,pt {
	    . Janeiro Fevereiro Mar\u00E7o Abril Maio Junho
	    Julho Agosto Setembro Outubro Novembro Dezembro
	}
	weekdays,pt {Dom Seg Ter Qua Qui Sex Sab}
	today,pt {}

	format,zh {%Y\u5e74 %m\u6708}
	mn,zh {
	    . \u4e00 \u4e8c \u4e09 \u56db \u4e94 \u516d \u4e03
	    \u516b \u4e5d \u5341 \u5341\u4e00 \u5341\u4e8c
	}
	weekdays,zh {\u65e5 \u4e00 \u4e8c \u4e09 \u56db \u4e94 \u516d}
	today,zh {}

	mn,fi {
	    . Tammikuu Helmikuu Maaliskuu Huhtikuu Toukokuu Kesäkuu
	    Heinäkuu Elokuu Syyskuu Lokakuu Marraskuu Joulukuu
	}
	weekdays,fi {Ma Ti Ke To Pe La Su}
	today,fi {}

	mn,tr {
	    . ocak \u015fubat mart nisan may\u0131s haziran temmuz a\u011fustos eyl\u00FCl ekim kas\u0131m aral\u0131k
	}
	weekdays,tr {pa'tesi sa \u00e7a pe cu cu'tesi pa}
	today,tr {}
    }
}

package provide widget::calendar 0.95
