/*
 * Initialize the default faces.
 */

sub make_face(ps_font_name, boldp, italicp)
{
  return list(ps_font_name, boldp, italicp, 0, 0);
}

fontname	= 0;
boldp		= 1;
italicp		= 2;
fg_color	= 3;
bg_color	= 4;

state default_faces
{
  BEGIN {
    face_bold 		= make_face(bold_font, 1, 0);
    face_italic		= make_face(italic_font, 0, 1);
    face_bold_italic	= make_face(bold_italic_font, 1, 1);
    face_comment	= make_face(italic_font, 0, 1);
    face_function_name	= make_face(bold_font, 1, 0);

    if (color)
      face_variable_name	= make_face(font, 0, 0);
    else
      face_variable_name	= make_face(bold_font, 1, 0);

    face_keyword	= make_face(bold_font, 1, 0);
    face_reference	= make_face(bold_font, 1, 0);
    face_string		= make_face(bold_font, 1, 0);
    face_builtin	= make_face(bold_font, 1, 0);
    face_type		= make_face(bold_font, 1, 0);
    face_highlight	= make_face(font, 0, 0);

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
