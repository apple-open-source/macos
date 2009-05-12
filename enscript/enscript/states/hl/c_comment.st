/*
 * Read one C-comment.  C-styled comments are needed in many
 * languages.  Therefore it is implemented in a separate file to ease
 * its reusing.
 */

state c_comment extends Highlight
{
  /\*\\\// {
    language_print ($0);
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
