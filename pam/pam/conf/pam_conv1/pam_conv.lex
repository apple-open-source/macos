
%{
/*
 * $Id: pam_conv.lex,v 1.2 2002/03/27 02:35:55 bbraun Exp $
 *
 * Copyright (c) Andrew G. Morgan 1997 <morgan@parc.power.net>
 *
 * This file is covered by the Linux-PAM License (which should be
 * distributed with this file.)
 */

    const static char lexid[]=
	"$Id: pam_conv.lex,v 1.2 2002/03/27 02:35:55 bbraun Exp $\n"
	"Copyright (c) Andrew G. Morgan 1997 <morgan@parc.power.net>\n";

    extern int current_line;
%}

%%

"#"[^\n]*         ; /* skip comments (sorry) */

"\\\n" {
    ++current_line;
}

([^\n\t ]|[\\][^\n])+ {
    return TOK;
}

[ \t]+      ; /* Ignore */

<<EOF>> {
    return EOFILE;
}

[\n] {
    ++current_line;
    return NL;
}

%%
