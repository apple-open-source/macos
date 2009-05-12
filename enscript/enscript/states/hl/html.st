/**
 * Name: html
 * Description: Hypertext markup language HTML.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state html_tag extends Highlight
{
  />/ {
    language_print ($0);
    return;
  }
  /\"/ {
    keyword_face (false);
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
    keyword_face (true);
  }
}

state html_entity extends Highlight
{
  /;/ {
    language_print ($0);
    return;
  }
}

state html_script_tag extends Highlight
{
  /\"/ {
    keyword_face (false);
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
    keyword_face (true);
  }
  /([lL][aA][nN][gG][uU][aA][gG][eE]=\")([^\"]*)(\")/ {
    html_script_language = $2;

    language_print ($1);
    keyword_face (false);

    string_face (true);
    language_print ($2);
    string_face (false);

    keyword_face (true);
    language_print ($3);
  }
  />/ {
    language_print ($0);
    return;
  }
}

state html_skip_script extends Highlight
{
  /* Terminator for nested scripts. */
  /<\/[sS][cC][rR][iI][pP][tT]>/ {
    from_html_terminator = $0;
    return;
  }
}

state html extends HighlightEntry
{
  BEGIN {
    require_state (javascript);
  }

  /* Scripts. */
  /<[sS][cC][rR][iI][pP][tT]/ {
    keyword_face (true);
    language_print ($0);

    /* The default script language is JavaScript. */
    html_script_language = "JavaScript";
    call (html_script_tag);
    keyword_face (false);

    if (regmatch(html_script_language, /^javascript.*$/i))
      {
	/* A nested JavaScript block. */
	from_html_terminator = "";
	call (javascript_internal);
	keyword_face (true);
	language_print (from_html_terminator);
	keyword_face (false);
      }
    else
      {
	/* An unknown scripting language, skip it. */
	from_html_terminator = "";
	call (html_skip_script);
	keyword_face (true);
	language_print (from_html_terminator);
	keyword_face (false);
      }
  }

  /</ {
    keyword_face (true);
    language_print ($0);
    call (html_tag);
    keyword_face (false);
  }
  /&/ {
    keyword_face (true);
    language_print ($0);
    call (html_entity);
    keyword_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
