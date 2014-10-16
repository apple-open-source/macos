package DateTime::Format::Simple;

use DateTime::Format::Builder (
    parsers => {
        parse_datetime => [
            {
                params => [qw( year month mday hours mins secs fsecs ampm )],
                regex  => qr[^
        (\d{4}) \s*-?\s* (\d{2}) \s*-?\s* (\d{2})
        \s*
        (?:-?\s* (\d{1,2}) :? (\d{2}) (?::? (\d{2}) )? )?
        (?:\. (\d+) ) ? # fsecs
        (?:\s* ([aApP]\.?[mM]\.?) )?
        $
        ]x,
            },
            {
                # mm/dd/yyyy, mm-dd-yyyy, [hh:mm[:ss[.nnn]]] [am/pm]
                params => [qw( month mday year hours mins secs fsecs ampm )],
                regex  => qr#^
        (\d{1,2})[-/](\d{1,2})[-/](\d{4})
        (?:\s+(\d{1,2}):(\d{2})(?::(\d{2}))?)?
        (?:\.(\d+))?
        (?:\s*([aApP]\.?[mM]\.?))?
        $
        #x
            },
        ]
    }
);
