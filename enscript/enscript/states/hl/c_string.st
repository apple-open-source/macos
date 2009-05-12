/*
 * Read one C-string.  C-styled strings are needed in many languages.
 * Therefore it is implemented in a separate file to ease its reusing.
 */

state c_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /\"/ {
    language_print ($0);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
