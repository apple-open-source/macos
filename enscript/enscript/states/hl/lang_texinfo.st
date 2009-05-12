
state lang_texinfo
{
  BEGIN {
    /* Nop since Texinfo doesn't have colors. */
    sub map_color (r, g, b)
      {
	return "";
      }

    LANGUAGE_SPECIALS = /[{}@]/;

    sub language_print (str)
      {
	str = regsuball (str, /@/, "@@");
	str = regsuball (str, /{/, "@{");
	str = regsuball (str, /}/, "@}");
	print (str);
      }

    sub language_symbol (symbol)
      {
	return false;
      }

    sub header ()
      {
	print ("@example\n");
      }

    sub trailer ()
      {
	print ("@end example\n");
      }

    sub face_on (face)
      {
	if (face[boldp])
	  print ("@strong{");
	if (face[italicp])
	  print ("@emph{");
      }

    sub face_off (face)
      {
	if (face[italicp])
	  print ("}");
	if (face[boldp])
	  print ("}");
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
