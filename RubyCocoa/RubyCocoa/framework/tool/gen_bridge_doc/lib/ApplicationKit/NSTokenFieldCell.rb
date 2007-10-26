module NSTokenFieldCellOverrides
  def new_definition
    # All the args are stringed together (so no spaces) in the original ref
    
    case @name
    when 'tokenFieldCell:displayStringForRepresentedObject:'
      return '- (NSString *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell displayStringForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:completionsForSubstring:indexOfToken:indexOfSelectedItem:'
      return '- (NSArray *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell completionsForSubstring:(NSString *)substring indexOfToken:(NSInteger)tokenIndex indexOfSelectedItem:(NSInteger *)selectedIndex'
    when 'tokenFieldCell:editingStringForRepresentedObject:'
      return '- (NSString *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell displayStringForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:editingStringForRepresentedObject:'
      return '- (NSString *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell editingStringForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:hasMenuForRepresentedObject:'
      return '- (BOOL)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell hasMenuForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:menuForRepresentedObject:'
      return '- (NSMenu *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell menuForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:readFromPasteboard:'
      return '- (NSArray *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell readFromPasteboard:(NSPasteboard *)pboard'
    when 'tokenFieldCell:representedObjectForEditingString:'
      return '- (id)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell representedObjectForEditingString:(NSString *)editingString'
    when 'tokenFieldCell:shouldAddObjects:atIndex:'
      return '- (NSArray *)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell shouldAddObjects:(NSArray *)tokens atIndex:(NSUInteger)index'
    when 'tokenFieldCell:styleForRepresentedObject:'
      return '- (NSTokenStyle)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell styleForRepresentedObject:(id)representedObject'
    when 'tokenFieldCell:writeRepresentedObjects:toPasteboard:'
      return '- (BOOL)tokenFieldCell:(NSTokenFieldCell *)tokenFieldCell writeRepresentedObjects:(NSArray *)objects toPasteboard:(NSPasteboard *)pboard'
    end
  end
end