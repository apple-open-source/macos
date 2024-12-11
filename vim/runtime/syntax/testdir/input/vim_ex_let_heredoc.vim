" Vim :let heredoc command
" VIM_TEST_SETUP let g:vimsyn_folding = "h"
" VIM_TEST_SETUP setl fdc=2 fdl=99 fdm=syntax


let foo =<< END
line1
line2
END

  let foo =<< END
line1
line2
END


" trim

let foo =<< trim END
  line1
  line2
END

  let foo =<< trim END
    line1
    line2
  END


" interpolation

let foo =<< eval END
line{1 + 0}
line{1 + 1}
END

  let foo =<< eval END
line{1 + 0}
line{1 + 1}
END

let foo =<< trim eval END
  line{1 + 0}
  line{1 + 1}
END

  let foo =<< trim eval END
    line{1 + 0}
    line{1 + 1}
  END

" no interpolation (escaped { and })

let foo =<< eval END
line{{1 + 0}}
line{{1 + 1}}
END

  let foo =<< eval END
line{{1 + 0}}
line{{1 + 1}}
END

let foo =<< trim eval END
  line{{1 + 0}}
  line{{1 + 1}}
END

  let foo =<< trim eval END
    line{{1 + 0}}
    line{{1 + 1}}
  END


" no interpolation

let foo =<< END
line{1 + 0}
line{1 + 1}
END

  let foo =<< END
line{1 + 0}
line{1 + 1}
END

let foo =<< trim END
  line{1 + 0}
  line{1 + 1}
END

  let foo =<< trim END
    line{1 + 0}
    line{1 + 1}
  END


" end marker must not be followed by whitespace

" assert_equal(foo, ["END "])
let foo =<< END
END 
END

" assert_equal(foo, [" END "])
let foo =<< END
 END 
END

" assert_equal(foo, ["END "])
let foo =<< trim END
  END 
END

" assert_equal(foo, ["END "])
  let foo =<< trim END
    END 
  END


" end marker must be vertically aligned with :let (if preceded by whitespace)

" assert_equal(foo, ["END"])
let foo =<< trim END
  END
END

  " assert_equal(foo, ["END"])
  let foo =<< trim END
    END
  END

" assert_equal(foo, ["END "])
let foo =<< trim END
END 
END

  " assert_equal(foo, ["END"])
  let foo =<< trim END
    END
  END

  " assert_equal(foo, ["END "])
  let foo =<< trim END
    END 
  END

  " assert_equal(foo, ["END"])
  let foo =<< trim END
     END
  END

  " assert_equal(foo, ["END "])
  let foo =<< trim END
     END 
  END

  " assert_equal(foo, ["END "])
  let foo =<< trim END
END 
END

  " assert_equal(foo, ["END"])
  let foo =<< trim END
 END
END

  " assert_equal(foo, ["END"])
  let foo =<< trim END
   END
END


" end markers

let foo =<< !@#$%^&*()_+
line1
line2
!@#$%^&*()_+

let foo =<< 0!@#$%^&*()_+
line1
line2
0!@#$%^&*()_+

let foo =<< A!@#$%^&*()_+
line1
line2
A!@#$%^&*()_+

" error - leading lowercase character
let foo =<< a!@#$%^&*()_+
line1
line2
a!@#$%^&*()_+

