namespace Container
{
  class Widget 
  {
  public:
    Widget (int inVal) : containerVar (inVal) {};
    int report (void) 
    { 
      return containerVar; 
    };
  private:
    int containerVar;
  };
} // namespace Container

namespace Container 
{
  namespace Contained 
  {
    class Widget : public Container::Widget 
    {
    public:
      Widget (int contVal, int baseVal) : Container::Widget (baseVal), contVar (contVal) {};
      int report (void) 
      {
	return contVar * Container::Widget::report ();
      };
    private:
      int contVar;
    };
  } // namespace Contained
} // namespace Container

int reportBase (Container::Widget *input)
{
  return input->report (); /* good stopping point in reportBase */
}

int main ()
{
  Container::Widget base (10);
  Container::Contained::Widget cont (100, 200);

  return reportBase (&cont) * cont.report(); /* good stopping point in main */

}

