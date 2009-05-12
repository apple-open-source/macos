/**
 * Name: verilog
 * Description: Verilog hardware description language
 * Author: Edward Arthur <eda@ultranet.com>
 */

state verilog extends HighlightEntry
{
  /* Verilog takes C++ style comments */

  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }
  /\/\// {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Macro expansions start with '`' and continue one word. */
  /`([a-zA-Z_0-9]+)/ {
    reference_face (true);
    language_print ($0);
    reference_face (false);
  }

  /* Keywords.
     (build-re '(always and assign begin buf bufif0 bufif1 case casex
     casez cmos deassign default defparam disable edge else end endcase
     endmodule endfunction endprimitive endspecify endtable endtask event
     for force forever fork function highz0 highz1 if initial inout input
     integer join large macromodule medium module nand negedge nmos nor
     not notif0 notif1 or output parameter pmos posedge primitive pull0
     pull1 pullup pulldown rcmos reg release repeat rnmos rpmos rtran
     rtranif0 rtranif1 scalared small specify specparam strength strong0
     strong1 supply0 supply1 table task time tran tranif0 tranif1 tri tri0
     tri1 triand trior trireg vectored wait wand weak0 weak1 while wire wor
     xnor xor
     $bitstoreal $countdrivers $display $fclose $fdisplay $fmonitor
     $fopen $fstrobe $fwrite $finish $getpattern $history $incsave $input
     $itor $key $list $log $monitor $monitoroff $monitoron $nokey $nolog
     $printtimescale $readmemb $readmemh $realtime $realtobits $reset
     $reset_count $reset_value $restart $rtoi $save $scale $scope
     $showscopes $showvariables $showvars $sreadmemb $sreadmemh $stime
     $stop $strobe $time $timeformat $write $vcdpluson $vcdplusoff
     $vcdplustraceon $vcdplustraceoff $dumpvars
     ;; prefix G stands for grave `
     Gaccelerate Gautoexpand_vectornets Gcelldefine Gdefault_nettype Gdefine
     Gelse Gendcelldefine Gendif Gendprotect Gendprotected
     Gexpand_vectornets Gifdef Ginclude Gnoaccelerate
     Gnoexpand_vectornets Gnoremove_gatenames Gnoremove_netnames
     Gnounconnected_drive Gprotect Gprotected Gremove_gatenames
     Gremove_netnames Gresetall Gtimescale Gunconnected_drive
     Guselib
     ))
   */
  /\$(bitstoreal|countdrivers|d(isplay|umpvars)\
|f(close|display|inish|monitor|open|strobe|write)|getpattern|history\
|i(n(csave|put)|tor)|key|l(ist|og)|monitor(|o(ff|n))|no(key|log)\
|printtimescale\
|r(e(a(dmem(b|h)|lt(ime|obits))|s(et(|_(count|value))|tart))|toi)\
|s(ave|c(ale|ope)|how(scopes|var(iables|s))|readmem(b|h)|t(ime|op|robe))\
|time(|format)|vcdplus(o(ff|n)|traceo(ff|n))|write)\b\
|`(a(ccelerate|utoexpand_vectornets)|celldefine|def(ault_nettype|ine)\
|e(lse|nd(celldefine|if|protect(|ed))|xpand_vectornets)|i(fdef|nclude)\
|no(accelerate|expand_vectornets|remove_(gatenames|netnames)\
|unconnected_drive)\
|protect(|ed)|re(move_(gatenames|netnames)|setall)|timescale\
|u(nconnected_drive|selib))\b\
|\b(a(lways|nd|ssign)|b(egin|uf(|if(0|1)))|c(ase(|x|z)|mos)\
|d(e(assign|f(ault|param))|isable)\
|e(dge|lse|nd(|case|function|module|primitive|specify|ta(ble|sk))|vent)\
|f(or(|ce|ever|k)|unction)|highz(0|1)|i(f|n(itial|out|put|teger))|join\
|large|m(acromodule|edium|odule)|n(and|egedge|mos|o(r|t(|if(0|1))))\
|o(r|utput)|p(arameter|mos|osedge|rimitive|ull(0|1|down|up))\
|r(cmos|e(g|lease|peat)|nmos|pmos|tran(|if(0|1)))\
|s(calared|mall|pec(ify|param)|tr(ength|ong(0|1))|upply(0|1))\
|t(a(ble|sk)|ime|r(an(|if(0|1))|i(|0|1|and|or|reg)))|vectored\
|w(a(it|nd)|eak(0|1)|hile|ire|or)|x(nor|or))\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
