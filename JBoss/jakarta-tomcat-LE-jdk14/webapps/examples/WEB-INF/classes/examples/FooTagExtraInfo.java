package examples;

import javax.servlet.jsp.tagext.*;

public class FooTagExtraInfo extends TagExtraInfo {
    public VariableInfo[] getVariableInfo(TagData data) {
        return new VariableInfo[] 
            {
                new VariableInfo("member",
                                 "String",
                                 true,
                                 VariableInfo.NESTED)
            };
    }
}

        
