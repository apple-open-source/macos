{ DEBUGGER.INC : Constantes des commandes du debugger FREETYPE }

const

  evWave             = $200;   { Broadcast messages }

  cmNewWin           = 200;
  cmFileOpen         = 201;

  cmNewExecution     = 300;
  cmRefocus          = 301;
  cmChangeRange      = 302;
  cmQueryCursorAddr  = 303;

  cmRun              = 400;
  kb_val_Run         = kbCtrlF9;
  kb_str_Run         = 'Ctrl-F9';

  cmGotoCursor       = 401;
  kb_val_GotoCursor  = kbF4;
  kb_str_GotoCursor  = 'F4';

  cmTraceInto        = 402;
  kb_val_TraceInto   = kbF7;
  kb_str_TraceInto   = 'F7';

  cmStepOver         = 403;
  kb_val_StepOver    = kbF8;
  kb_str_StepOver    = 'F8';

  cmUntilReturn      = 404;
  kb_val_UntilReturn = kbAltF8;
  kb_str_UntilReturn = 'Alt-F8';

  cmToggleBreak      = 500;
  kb_val_ToggleBreak = kbCtrlF8;

  cmClearBreaks = 501;

  cmViewGlyph      = 600;
  kb_val_ViewGlyph = kbF9;
  kb_str_ViewGlyph = 'F9';
