
  { Restores screen to the original state }

  function Driver_Restore_Mode: boolean;
  begin
    asm
      mov ax, $0003
      int $10
    end;
    Driver_Restore_Mode := True;
  end;

  function Driver_Set_Graphics( mode : Int ) : boolean;
  var
    rc : Int;
  begin
    Driver_Set_Graphics := False;

    rc := 0;

    case Mode of

      Graphics_Mode_Mono : begin
                             asm
                               mov ax, $0012
                               int $10
                             end;
                             Vio_ScanLineWidth := 80;
                             Vio_Width         := 640;
                             Vio_Height        := 480;
                           end;

      Graphics_Mode_Gray : begin
                             asm
                               mov ax, $0013
                               int $10
                             end;
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

    if rc <> 0 then exit;

    Vio := @Mem[$A000:0];

    Driver_Set_Graphics := True;
  end;


  procedure Driver_Display_Bitmap; assembler;
  asm
    push ds
    push bp

    les di, [Vio]

    cld

    mov cx,[Vio_ScanLineWidth]
    mov bx,[col]
    mov ax,[line]
    dec ax
    mul cx
    add di,ax
    mov dx,[line]

    lds si,[Buff]

    mov bp,bx
    add bx,cx

   @1:
    mov cx,bp
    rep movsb
    sub di,bx
    dec dx
    jnz @1

    pop bp
    pop ds
  end;


