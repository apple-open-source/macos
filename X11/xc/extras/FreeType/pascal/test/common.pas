Unit Common;

interface

const
  Max_Arguments = 1024;

var
  num_arguments  : integer;
  (* the number of arguments contained in the 'arguments' array *)

  arguments      : array[0..Max_Arguments-1] of ^string;
  (* This array will hold all arguments after wildcard expansion *)
  (* note that it will not contain the original arguments that   *)
  (* were before 'first_argument' of Expand_Wildcards            *)

  procedure Expand_WildCards( first_argument    : integer;
                              default_extension : string );
  (* expand all wildcards into filenames *)

implementation

uses Dos;

  procedure Split( Original : String;
                   var Base : String;
                   var Name : String );
  var
    n : integer;
  begin
    n := length(Original);

    while ( n > 0 ) do
      if ( Original[n] = '\' ) or
         ( Original[n] = '/' ) then
        begin
          Base := Copy( Original, 1, n-1 );
          Name := Copy( Original, n+1, length(Original) );
          exit;
        end
      else
        dec(n);

    Base := '';
    Name := Original;
  end;


  procedure Expand_WildCards( first_argument    : integer;
                              default_extension : string );
  var
    i, n       : integer;
    base, name : string;
    SRec       : SearchRec;
  begin
    num_arguments := 0;
    i             := first_argument;

    while ( i <= ParamCount ) do
    begin
      Split( ParamStr(i), base, name );
      if base <> '' then
        base := base + '\';

      FindFirst( base+name, Archive+ReadOnly+Hidden, SRec );
      if DosError <> 0 then
        FindFirst( base+name+default_extension, AnyFile, SRec );

      while (DosError = 0) and (num_arguments < Max_Arguments) do
      begin
        GetMem( arguments[num_arguments], length(base)+length(SRec.Name)+1 );
        arguments[num_arguments]^ := base + SRec.Name;
        inc( num_arguments );
        FindNext( SRec );
      end;

      {$IFDEF OS2}
      FindClose( SRec );
      {$ENDIF}
      inc( i );
    end;
  end;

end.
