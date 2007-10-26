module ABSearchElementOverrides
  def new_definition
    return '+ (ABSearchElement *)searchElementForConjunction:(ABSearchConjunction)conjunction children:(NSArray *)children' if @name == 'searchElementForConjunction:children:'
  end
end