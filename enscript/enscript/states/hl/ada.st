/**
 * Name: ada
 * Description: Ada95 programming language.
 * Author: Rolf Ebert <ebert@waporo.muc.de>
 */

state ada extends HighlightEntry
{
  /* Comments. */
  /--/ {
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


  /* Character constants. */
  /'.'|'\\\\.'/ {
    string_face (true);
    language_print ($0);
    string_face (false);
  }

  /* Keywords.
     (build-re '(abort abs abstract accept access aliased
     all and array at begin body case constant declare
     delay delta digits do else elsif end entry
     exception exit for function generic goto if in
     is limited loop mod new not null of or others
     out package pragma private procedure protected raise
     range record rem renames requeue return reverse
     select separate subtype tagged task terminate then type
     until use when while with xor))
   */
  /\b(a(b(ort|s(|tract))|cce(pt|ss)|l(iased|l)|nd|rray|t)|b(egin|ody)\
|c(ase|onstant)|d(e(clare|l(ay|ta))|igits|o)\
|e(ls(e|if)|n(d|try)|x(ception|it))|f(or|unction)|g(eneric|oto)\
|i(f|n|s)|l(imited|oop)|mod|n(ew|ot|ull)|o(f|r|thers|ut)\
|p(ackage|r(agma|ivate|o(cedure|tected)))\
|r(a(ise|nge)|e(cord|m|names|queue|turn|verse))\
|s(e(lect|parate)|ubtype)|t(a(gged|sk)|erminate|hen|ype)|u(ntil|se)\
|w(h(en|ile)|ith)|xor)\b/ {
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
