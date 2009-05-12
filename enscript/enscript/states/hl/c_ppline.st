/*
 * C preprocessor line handling.  This is shared between C, C++, and
 * Objective-C highlighting rules.
 */

state c_ppline extends CHighlight
{
  BEGIN {
    require_state (c);
  }
  /* Comments within a pre-processor line. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }
  /* Include line. */
  /(include)([ \t]+)/ {
    reference_face (true);
    language_print ($1);
    reference_face (false);
    language_print ($2);
    call (c_ppline_include);
    return;
  }
  /* Define line. */
  /(define)([ \t]+)/ {
    reference_face (true);
    language_print ($1);
    reference_face (false);
    language_print ($2);
    call (c_ppline_define);
    return;
  }
  /* Simple directives:
     (build-re '(undef if ifdef ifndef endif elif else line error pragma))
   */
  /\b(e(l(if|se)|ndif|rror)|if(|def|ndef)|line|pragma|undef)\b/ {
    reference_face (true);
    language_print ($0);
    reference_face (false);
    call (c_ppline_tokensequence);
    return;
  }
  /* An unknown pre-processor directive. */
  /[a-zA-Z_][^ \t\n]*/ {
    reference_face (true);
    language_print ($0);
    reference_face (false);
    call (c_ppline_tokensequence);
    return;
  }
  /\n/ {
    language_print ($0);
    return;
  }
}

state c_ppline_include extends CHighlight
{
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
    call (c_ppline_comments_strings_chars);
    return;
  }
  /<[^<>]+>/ {
    string_face (true);
    language_print ($0);
    string_face (false);
    call (c_ppline_comments_strings_chars);
    return;
  }
  /[a-zA-Z_][a-zA-Z_0-9]*/ {
    variable_name_face (true);
    print ($0);
    variable_name_face (false);
    call (c_ppline_comments_strings_chars);
    return;
  }
  /\n/ {
    language_print ($0);
    return;
  }
}

state c_ppline_define extends CHighlight
{
  /([a-zA-Z_][a-zA-Z_0-9]*)(\([^\)]*\))?/ {
    if (strcmp ($2, "") != 0)
      {
	function_name_face (true);
	language_print ($1);
	function_name_face (false);
	language_print ($2);
      }
    else
      {
	variable_name_face (true);
	language_print ($1);
	variable_name_face (false);
      }
    call (c_ppline_comments_strings_chars);
    return;
  }
  /\n/ {
    language_print ($0);
    return;
  }
}

state c_ppline_comments_strings_chars extends CHighlight
{
  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
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
  /\n/ {
    language_print ($0);
    return;
  }
}

state c_ppline_tokensequence extends CHighlight
{
  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
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
  /* defined() operators. */
  /(defined)(\()([^\)]+)(\))/ {
    reference_face (true);
    language_print ($1);
    reference_face (false);
    language_print ($2);

    variable_name_face (true);
    language_print ($3);
    variable_name_face (false);

    language_print ($4);
  }
  /* Variable references. */
  /\b[a-zA-Z_][a-zA-Z_0-9]*\b/ {
    variable_name_face (true);
    language_print ($0);
    variable_name_face (false);
  }
  /\n/ {
    language_print ($0);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
