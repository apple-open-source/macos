# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/ja/message_proc.tcl,v 1.2 1998/04/05 15:30:33 robin Exp $
#
# These procedures generate local messages with arguments

proc make_message_phase4 { saveto } {
    global messages
    set messages(phase4.2) \
	    "ファイル $saveto のバックアップファイル、$saveto.bak が\n\
	    作れないので、設定は保存されませんでした。\n\
	    ファイル名を変更して、保存し直して下さい。"
    set messages(phase4.3) "今までの設定ファイルは $saveto.bak に \n\
	    バックアップとして保存されました。"
    set messages(phase4.4) \
	    "ファイル $saveto に設定を保存することができません。\n\n\
	    ファイル名を変更して保存し直して下さい"
    set messages(phase4.5) "X の設定が完了しました。\n\n"
}
proc make_message_card { args } {
    global pc98 messages Xwinhome
    global cardServer
    
    set mes ""
    if !$pc98 {
	if ![file exists $Xwinhome/bin/XF86_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
			"!!! このグラフィックカードに必要なサーバーは\
			インストールされていません。設定を中断して、\
			$cardServer サーバーを $Xwinhome/bin/XF86_$cardServer \
			の名前でインストールし、もう一度設定を \
			やり直して下さい。 !!!"
	    } else {
		set mes \
			"!!! 選択されたサーバーはインストール \
			されていません。設定を中断して、\
			$cardServer サーバーを $Xwinhome/bin/XF86_$cardServer \
			の名前でインストールし、もう一度設定を \
			やり直して下さい。 !!!"
	    }
	    bell
	}
    } else {
	if ![file exists $Xwinhome/bin/XF98_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
			"!!! このグラフィックカードに必要なサーバーは\
			インストールされていません。設定を中断して、\
			$cardServer サーバーを $Xwinhome/bin/XF98_$cardServer \
			の名前でインストールし、もう一度設定を \
			やり直して下さい。 !!!"
	    } else {
		set mes \
			"!!! 選択されたサーバーはインストール \
			されていません。設定を中断して、\
			$cardServer サーバーを $Xwinhome/bin/XF98_$cardServer \
			の名前でインストールし、もう一度設定を \
			やり直して下さい。 !!!"	
	    }
	    bell
	}
    }
    return $mes
}

proc make_intro_headline { win } {
    global pc98
    $win tag configure heading \
	    -font -jis-fixed-medium-r-normal--24-230-*-*-c-*-jisx0208.1983-0
    if !$pc98 {
	$win insert end \
		"ＸＦ８６Ｓｅｔｕｐについて" heading
    } else {
	$win insert end \
		"ＸＦ９８Ｓｅｔｕｐについて" heading
    }
}

proc make_underline { win } {
	$win.menu.mouse configure -underline 4
	$win.menu.keyboard configure -underline 6
	$win.menu.card configure -underline 4
	$win.menu.monitor configure -underline 7
	$win.menu.modeselect configure -underline 4
	$win.menu.other configure -underline 4
	$win.buttons.abort configure -underline 3
	$win.buttons.done configure -underline 5
	$win.buttons.help configure -underline 4
}
