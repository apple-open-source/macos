/**
 * Name: awk
 * Description: AWK programming language.
 * Author: Juergen Kahrs <Juergen.Kahrs@t-online.de>
 */

state awk extends HighlightEntry
{
  /* Comments. */
  /#/ {
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

  /* Excutable script. */
  /^#!/ {
    reference_face (true);
    language_print ($0);
    call (eat_one_line);
    reference_face (false);
  }

  /* Keywords. */
  /\b(ARG(C|V|IND)|BEGIN|CONVFMT|E(N(D|VIRON)|RRNO)\
|F(I(ELDWIDTHS|LENAME)|NR|S)|IGNORECASE|N[FR]|O(FMT|FS|RS)\
|R(LENGTH|S(TART)?|T)|SUBSEP\
|atan2|break|c(lose|o(ntinue|s))|d(elete|o)|e(lse|x(it|p))\
|f(flush|or|unction)|g(((en)?sub)|etline)|i(f|n(dex|t)?)\
|l(ength|og)|match|next(file)?|return|while|print[f]?\
|rand|s(in|ub(str)?|ystem|p(lit|rintf)|qrt|rand|trftime|ystime)\
|to(lower|upper))\b/ {
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
