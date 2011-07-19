[example {
PEG calculator (Expression)
    Digit      <- '0'/'1'/'2'/'3'/'4'/'5'/'6'/'7'/'8'/'9'	;
    Sign       <- '-' / '+'					;
    Number     <- Sign? Digit+					;
    Expression <- '(' Expression ')' / (Factor (MulOp Factor)*)	;
    MulOp      <- '*' / '/'					;
    Factor     <- Term (AddOp Term)*				;
    AddOp      <- '+'/'-'					;
    Term       <- Number					;
END;
}]
