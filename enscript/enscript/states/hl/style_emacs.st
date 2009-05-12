
state style_emacs
{
  BEGIN {
    call(default_faces);

    if (color)
      {
	/* Set colors. */
	face_bold[fg_color] =		language_color("black");
	face_italic[fg_color] =		language_color("black");
	face_bold_italic[fg_color] =	language_color("black");
	face_comment[fg_color] =	language_color("firebrick");
	face_function_name[fg_color] =	language_color("blue");
	face_variable_name[fg_color] =	language_color("dark goldenrod");
	face_keyword[fg_color] =	language_color("purple");
	face_reference[fg_color] =	language_color("cadet blue");
	face_string[fg_color] =		language_color("rosy brown");
	face_builtin[fg_color] =	language_color("orchid");
	face_type[fg_color] =		language_color("forest green");
	face_highlight[bg_color] =	language_color("DarkSeaGreen2");
      }
    else
      {
	face_highlight[bg_color] =	language_color("LightGrey");
      }

    /* And that's it. */
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
