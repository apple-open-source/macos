
state lang_overstrike
{
  BEGIN {
    sub map_color (r, g, b)
      {
	return "";
      }

    LANGUAGE_SPECIALS = /./;

    sub language_print (str)
      {
	if (italic_flag)
	  print (regsuball (str, /[A-Za-z0-9]/, "_\010$0"));
	else if (bold_flag)
	  print (regsuball (str, /[^\n]/, "$0\010$0"));
	else
	  print (str);
      }

    sub language_symbol (symbol)
      {
	return false;
      }

    sub header ()
      {
	bold_flag = 0;
	italic_flag = 0;
      }

    sub trailer ()
      {
      }

    sub face_on (face)
      {
	if (face[boldp])
	  bold_flag = 1;
	if (face[italicp])
	  italic_flag = 1;
      }

    sub face_off (face)
      {
	if (face[boldp])
	  bold_flag = 0;
	if (face[italicp])
	  italic_flag = 0;
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
