
  {$IFDEF DYNAMIC_VERSION}
    {$Dynamic System}
    {$L VPRTL.LIB}
  {$ENDIF}


  type
    Ptr16Rec = record
      Ofs,Sel: SmallWord;
    end;

  var
    OrgMode   : VioModeInfo;
    VioBufOfs : Longint;
    Status    : SmallWord;

  { BIOS Video Mode
#13 }

  const
    VioMode_640x480x16 : VioModeInfo =
     ( cb:     SizeOf(VioModeInfo);
       fbType: vgmt_Other + vgmt_Graphics;
       Color:  colors_16;
       Col:    80;
       Row:    35;
       HRes:   640;
       VRes:   480
     );

    VioMode_320x200x256 : VioModeInfo =
     ( cb:     SizeOf(VioModeInfo);
       fbType: vgmt_Other + vgmt_Graphics;
       Color:  colors_256;
       Col:    40;
       Row:    25;
       HRes:   320;
       VRes:   200
     );

    VioBuf: VioPhysBuf =
     ( pBuf: Ptr($A0000);
       cb:   64*1024
     );

  { Restores screen to the original state }

  function Driver_Restore_Mode: boolean;
  begin
    VioSetMode(OrgMode, 0);
    Driver_Restore_Mode := True;
  end;

  function Driver_Set_Graphics( mode : Int )
 : boolean;
  var
    rc : Int;
  begin
    Driver_Set_Graphics := False;

    { Save original video mode }
    OrgMode.cb := SizeOf(VioModeInfo);
    VioGetMode(OrgMode, 0);

    case Mode of

      Graphics_Mode_Mono : begin
           rc := VioSetMode( VioMode_640x480x16, 0 );
                             Vio_ScanLineWidth := 80;
                             Vio_Width         := 640;
                             Vio_Height        := 480;
                           end;

      Graphics_Mode_Gray : begin
                             rc := VioSetMode( VioMode_320x200x256, 0 );
                             Vio_ScanLineWidth := 320;
                             Vio_Width         := 320;
                             Vio_Height        := 200;

                             (* default gray palette takes the gray levels *)
                             (* the standard VGA 256 colors mode           *)

                             gray_palette[0] := 0;
                             gray_palette[1] := 23;
                             gray_palette[2] := 27;
                             gray_palette[3] := 29;
                             gray_palette[4] := 31;
                           end;
    else
      rc := -1;
    end;

    { Set VGA 640x400x16
 video mode }
    if rc <> 0 then exit;

    { Get selector for physical video buffer }
    if VioGetPhysBuf(VioBuf, 0) <> 0 then exit;

    { Make flat pointer that points to the physical video buffer}
    Ptr16Rec(VioBufOfs).Ofs := 0;
    Ptr16Rec(VioBufOfs).Sel := VioBuf.Sel;
    SelToFlat(Pointer(VioBufOfs));

    { Clear the screen. Unlike function 0 of the BIOS INT 10h }
    { VioSetMode doesn't clear the screen.                    }
    FillChar(Pointer(VioBufOfs)^,64*1024,0);
    Vio := PVioScreenBuffer(VioBufOfs);

    Driver_Set_Graphics := True;
  end;


  procedure Driver_Display_Bitmap; assembler;
  asm
    push esi
    push edi
    push ebx
    push ecx

    mov esi,[Buff]

    mov ecx,[Vio_ScanLineWidth]
    mov ebx,[Col]
    mov eax,[Line]

    dec eax
    mul ecx

    mov edi,[VioBufOfs]
    add edi,eax

    mov edx,[line]
    add ebx,[Vio_ScanLineWidth]
   @1:
    mov ecx,[col]
    rep movsb
    sub edi,ebx
    dec edx
    jnz @1

    pop ecx
    pop ebx
    pop edi
    pop esi
  end;

