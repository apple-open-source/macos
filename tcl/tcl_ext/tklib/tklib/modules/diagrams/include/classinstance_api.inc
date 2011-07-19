[subsection {Class API}]

The package exports the API described here.

[list_begin definitions]

	[call [cmd ::diagram] [arg objectName] [arg canvas] [opt [arg script]]]

		The command creates a new instance of a diagram
		controller and returns the fully qualified name of the
		object command as its result.

		The new instance is connected to the specified
		[arg canvas] object, which is used as the diagrams
		graphics engine. This is usually an instance of Tk's
		canvas, however any object which is API compatible to
		Tk's canvas can be used here.

	[para]

		The API of this object command is described in the
		following section, [sectref {Object API}]. It may be
		used to invoke various operations on the object.

	[para]

		If the [arg script] argument is specified then method
		[method draw] will be invoked on it.

[list_end]

[subsection {Object API}]

	    Instances of the diagram class support the following
	    methods:

[list_begin definitions]

	[call [arg diagramObject] [method {new direction}] [arg name] [opt "[arg key] [arg value]..."]]

		This method defines a new named direction and its
		attributes. The latter is given through the
		[arg key]/[arg value] pairs coming after the [arg name].

	[para]

		Users are mostly free to specify arbitrary attributes
		with whatever meaning they desire. The exception are
		the names [emph angle] and [emph opposite]. They are
		special to the diagram package and have a fixed meaning.

	[list_begin definitions]
	[def angle]

		This attribute specifies the angle of the direction in
		degrees, where 0 points east (to the right) and 90 points
		north (up).

	[def opposite]

		This attribute specifies the name of the direction
		which should be considered as complementary to the
		named one.

	[list_end]


	[call [arg diagramObject] [method {new element}] [arg name] [arg attributes] [arg cmdprefix]]

		This method defines a new graphics element for the
		drawing language. I.e. [arg name] will become a new
		command in the language, and the specified command
		prefix ([arg cmdprefix]) will be called to perform
		the actual drawing.

	[para]
		[arg attributes] specifies the set of attributes for which
		data has to be available. I.e. the system will run the
		...-callbacks for these attributes.

		See the method [method {new attribute}] for more information
		on attribute definitions.

	[para]

		The command prefix is expected to conform to the
		following signature:

	[list_begin definitions]
	[def "[cmd cmdprefix] [arg canvas] [arg attributes]"]

		Where [arg canvas] is the handle of the canvas widget
		to draw to, and [arg attributes] is a dictionary
		holding the attributes for the element, be they
		user-specified, or defaults.

	[para]
		The results of the command has to be a list containing
		at least two and at most four items. These are, in order:

		[list_begin enumerated]
		[enum]	The list of canvas items the drawn element consists of.
		[enum]	The dictionary of named locations in the element, its
			[term corners].
		[enum]	An optional mode, either "relative" or "absolute".
			When not returned "relative" is assumed. In the
			case of a relative element position the attributes
			"with" and "at" are used to determine the final
			position of the new element.
		[enum]	An optional name of a direction. If not the
			empty string this is handed to the automatic
			layouter as the new direction to follow.
		[list_end]
	[list_end]


	[call [arg diagramObject] [method {new alias}] [arg name] [arg cmdprefix]]

		This method defines a new command for the drawing
		language. I.e. [arg name] will become a new command in
		the language, and the specified command prefix
		([arg cmdprefix]) will be called on use of this new
		command. Any arguments given to the command are
		simply passed to the prefix. There is no fixed siganture.

	[para]
		Note that the prefix is run in the context of the
		drawing language, allowing the direct use of any
		existing commands.


	[call [arg diagramObject] [method {new command}] [arg name] [arg arguments] [arg body]]

		This is like [method {new alias}] except that the new
		command is defined as a procedure in the language's
		context, with regular argument list and body.

	[call [arg diagramObject] [method {new attribute}] [arg name] [opt "[arg key] [arg value]..."]]

		This method defines a new named attribute which can be
		used by graphical elements. The handling of the
		attribute by the processor is declared through the
		[arg key]/[arg value] pairs coming after the [arg name].

	[para]
		The accepted keys and their meanings are:

		[list_begin definitions]

		[def [const key]]

			The value of this key is the name of the key
			under which the attribute's value shall be
			stored in the attribute dictionary given to
			the drawing command after attribute processing
			is complete.

		[para]

			This key is optional. If it is not specified it
			defaults to the name of the attribute.

		[def [const get]]

			The value of this key is a command prefix
			which will be invoked to retrieve the
			attribute's argument(s) from the command
			line.

		[para]

			This key is optional. If it is not specified a
			default is used which takes the single word
			after the attribute name as the attribute's
			value.

		[para]
			The signature of the command prefix is

			[list_begin definitions]
			[def "[cmd cmdprefix] [arg wordqueue]"]

				Where [arg wordqueue] is the handle of
				a queue object conforming to the API
				of the queues provided by package
				[package struct::queue]. This queue
				contains the not-yet-processed part of
				the attribute definitions, with one
				entry per word, with the first entry
				the word [emph after] name of the
				attribute. In other words, the
				attribute's name has already been
				removed from the queue.

			[para]
				The result of the command is the value
				of the attribute, which may have been
				taken from the queue, or not.

			[list_end]


		[def [const transform]]

			The value of this key is a command prefix
			which will be invoked to transform the
			retrieved value (See [const get]) into their
			final form.

		[para]

			This key is optional. If it is not specified
			no transformation is done.

			The signature of the command prefix is

			[list_begin definitions]
			[def "[cmd cmdprefix] [arg value]"]

				Where [arg value] is the value to transform.

			[para]

				The result of the command is the final
				value of the attribute.

			[list_end]

		[def [const type]]

			The value of this key is a command prefix
			which will be invoked to validate the
			attribute's argument(s).

		[para]

			This key is optional. If it is not specified
			no validation is done.

		[para]

			The signature of the command prefix is that of
			snit validation types. See the documentation
			of the [package snit] package.

		[def [const merge]]

			The value of this key is a command prefix
			which will be invoked to insert the
			transformed and validated attribute value into
			the dictionary of collected attributes.

		[para]

			This key is optional. If it is not specified
			a default merge is chosen, based on the data
			for key [const aggregate], see below.

			The signature of the command prefix is

			[list_begin definitions]
			[def "[cmd cmdprefix] [arg value] [arg dict]"]

				Where [arg value] is the value to
				insert, and [arg dict] the dictionary
				of attributes and values collected so
				far.

			[para]

				The result of the command is the new
				dictionary of attributes.

			[list_end]

		[def [const aggregate]]

			The value of this key is a boolean flag. It
			has an effect if and only if the key [const \
			merge] was not specified. This key is
			optional. If it is not specified it defaults
			to [const False].

		[para]

			If the key is effective, the value of [const \
			False] means that the attribute's value is
			[emph set] into the dictionary using the value
			of key [arg key] (see above) as index,
			[emph overwriting] any previously specified value.

		[para]
			If the key is effective, the value of [const \
			True] means that the attribute's value is
			[emph added] to the dictionary using the value
			of key [arg key] (see above) as index,
			[emph extending] any previously specified value.

			This means that the final value of the
			attribute as seen after processing will be a
			list of the collected values.

		[def [const default]]

			The value of this key is a command prefix
			which will be invoked after collection of
			attributes has been completed and this
			attribute is in the list of required
			attributes for the drawing element (See
			argument [arg attributes] of method
			[method {new element}]).

		[para]

			Note that the connection is made through the
			value of key [arg key], not through the
			attribute name per se.

		[para]

			Further note that this command prefix is
			invoked even if a user specified attribute
			value is present. This allows the command
			to go beyond simply setting defaults, it
			can calculate and store derived values as
			well.

		[para]

			This key is optional. If an element requires
			this attribute, but [arg default] is not
			specified then nothing will be done.

		[para]
			The signature of the command prefix is

			[list_begin definitions]
			[def "[cmd cmdprefix] [method init]"]

				This method is run when the attribute
				is defined, its responsibility is to
				initialize anything in the language
				namespace for the attribute and
				default processing.
			[para]
				The result of this method is ignored.

			[def "[cmd cmdprefix] [method fill] [arg varname]"]

				This method is run to put defaults, or
				derived values into the attribute dictionary
				named by [arg varname]. This variable will
				be found in the calling context.
			[para]
				The result of this method is ignored.

			[def "[cmd cmdprefix] [method set] [arg name] [arg value]"]

				This method is run to push current a
				attribute value into the language
				namespace, to make it the new default.

			[para]
				The result of this method is ignored.

			[list_end]

		[def [const linked]]

			This key is effective if and only if key
			[const default] is not specified. In that
			case is supplies a default handling for
			[const default], linking the attribute to a
			variable in the language context.

		[para]
			The value for this key is a 2-element list
			containing the name of the variable to link
			to, and its initial value, in this order.

		[list_end]

	[call [arg diagramObject] [method {unknown attribute}] [arg cmdprefix]]

		This method registers the command prefix with the
		subsystem processing the attributes for element
		commands, telling it to call it when it encounters an
		attribute it is unable to handle on its on.

	[para]

		It is allowed to register more than callback, these
		will be called in order of registration (i.e. first to
		last), until one of the callbacks accepts the current
		input.

		The command prefix is expected to conform to the
		following signature:

		[list_begin definitions]
		[def "[cmd cmdprefix] [arg wordqueue]"]

			Where [arg wordqueue] is the handle of a queue
			object conforming to the API of the queues
			provided by package [package struct::queue].
			This queue contains the not-yet-processed part
			of the attribute definitions, with one entry
			per word, with the first entry the name of the
			attribute which could not be processed.

		[para]

			The results of the command has to be a boolean
			value where [const True] signals that this
			callback has accepted the attribute, processed
			it, and the new state of the [arg wordqueue]
			is where the general processing shall continue.

		[para]

			Given the signature the command has basically
			two ways of handling (rewriting) the attributes
			it recognizes:

			[list_begin enumerated]
			[enum]	Replace the attribute (and arguments)
				with a different attribute and arguments.
			[enum]	Push additional words in front to get
				the general processing unstuck.
			[list_end]
		[list_end]


	[call [arg diagramObject] [method draw] [arg script]]

		This method runs the given [arg script] in the
		context of the drawing language definitions.
		See section [sectref {Language Reference}] for
		details on the available commands.

	[para]
		[emph Note] that [arg script] is [emph trusted].
		It is executed in the current interpreter with
		access to its full abilities.

		For the execution of untrusted diagram scripts this
		interpreter should be a safe one.
[list_end]

[comment {
[subsection {Element/Attribute Processing}]

[image examples/figure-00-eap]

}]
