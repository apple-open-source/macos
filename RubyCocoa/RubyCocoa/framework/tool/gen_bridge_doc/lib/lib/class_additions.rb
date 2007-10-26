module Hpricot
  class Elem
    def fits_the_description?(tag, text)
      self.name == tag and self.inner_html.include?(text)
    end
    def spaceabove?
      self.get_attribute('class') == 'spaceabove'
    end
    def spaceabovemethod?
      self.get_attribute('class') == 'spaceabovemethod'
    end
    def spacer?
      self.spaceabove? or self.spaceabovemethod?
    end
  end
end
