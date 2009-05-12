/**
 * Name: changelog
 * Description: ChangeLog files.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state changelog extends HighlightEntry
{
  /* Date entries.  Both new and old formats. */
  /^([^ \t]........[0-9: ]*)([^<]+)(<)([A-Za-z0-9_.-]+@[A-Za-z0-9_.-]+)(>)/ {
    string_face (true);
    language_print ($1);
    string_face (false);

    reference_face (true);
    language_print ($2);
    reference_face (false);

    language_print ($3);

    variable_name_face (true);
    language_print ($4);
    variable_name_face (false);

    language_print ($5);
  }

  /* File descriptions with function names. */
  /(^\t\* )([^\(]+)(\()([^\)]+)(\):)/ {
    language_print ($1);

    function_name_face (true);
    language_print ($2);
    function_name_face (false);

    language_print ($3);

    keyword_face (true);
    language_print ($4);
    keyword_face (false);

    language_print ($5);
  }

  /* File descriptions without function names. */
  /(^\t\* )([^ :]+)(:)/ {
    language_print ($1);

    function_name_face (true);
    language_print ($2);
    function_name_face (false);

    language_print ($3);
  }

  /* Function name descriptions without file names. */
  /(^\t\()([^\)]+)(\):)/ {
    language_print ($1);

    keyword_face (true);
    language_print ($2);
    keyword_face (false);

    language_print ($3);
  }
}


/*
Local variables:
mode: c
End:
*/
