/*
 * Parser errors
 *
 * Total errors: 8 (+1 = 9)
 */

# Too many arguments (1)
frop :this "is" "a" 2 :long "argument" "list" :and :it :should "fail" :during "parsing" :but "it" "should" "be" 
	"recoverable" "." :this "is" "a" 2 :long "argument" "list" :and :it :should "fail" :during "parsing" :but 
	"it" "should" "be" "recoverable" {
	stop;
}

# Garbage argument (2)
friep $$$;

# Deep block nesting (1)
if true { if true { if true { if true { if true { if true { if true { if true {
	if true { if true { if true { if true { if true { if true { if true { if true {
		if true { if true { if true { if true { if true { if true { if true { if true {
			if true { if true {	if true { if true {	if true { if true { if true { if true {
				if true { if true { if true { if true { if true { if true { if true { if true {
					stop;
				} } } } } } } }
			} } } } } } } }
		} } } } } } } }
	} } } } } } } }
} } } } } } } }

# Deepest block and too deep test (list) nesting (1)
if true { if true { if true { if true { if true { if true { if true { if true {
	if true { if true { if true { if true { if true { if true { if true { if true {
		if true { if true { if true { if true { if true { if true { if true { if true {
			if true { if true {	if true { if true {	if true { if true { 
				if	
					anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( 
					anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( 
					anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( 
					anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( 
					anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof ( anyof (
						true 
					)))))))) 
					)))))))) 
					)))))))) 
					)))))))) 
					)))))))) 
				{
					stop;
				} 
			} } } } } }
		} } } } } } } }
	} } } } } } } }
} } } } } } } }

# Deepest block and too deep test nesting (1)
if true { if true { if true { if true { if true { if true { if true { if true {
	if true { if true { if true { if true { if true { if true { if true { if true {
		if true { if true { if true { if true { if true { if true { if true { if true {
			if true { if true {	if true { if true {	if true { if true { 
				if	
					not not not not not not not not
					not not not not not not not not
					not not not not not not not not
					not not not not not not not not
					not not not not not not not not false
				{
					stop;
				} 
			} } } } } }
		} } } } } } } }
	} } } } } } } }
} } } } } } } }


# Garbage command; test wether previous errors were resolved (2)
frop $$$$;


