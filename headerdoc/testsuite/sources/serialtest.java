
/** This is an I/O Class */
class File implements java.io.Serializable {
    /** @serial
	This is a serial field description.
     */
    int a;

    private String[] pathcomponents;
    // Define serializable fields with the ObjectStreamClass
    


        /**


         * @serialField path String 


         *               Path components separated by separator.          


         */


        private static final ObjectStreamField[] serialPersistentFields 
            =  new ObjectStreamField("path", String.class);

    /**


     * @serialData  Default fields followed by separator character.             


     */ 


    private void writeObject(ObjectOutputStream s)
        throws IOException
    {
        ObjectOutputStream.PutField fields = s.putFields();
        StringBuffer str = new StringBuffer();
        for(int i = 0; i < pathcomponents; i++) {
            str.append(separator);
            str.append(pathcomponents[i]);
        }
        fields.put("path", str.toString());
        s.writeFields();
        s.writeChar(separatorChar); // Add the separator character
    }

    private void readObject(ObjectInputStream s)
        throws IOException
    {
        ObjectInputStream.GetField fields = s.readFields();
        String path = (String)fields.get("path", null);
        char sep = s.readChar(); // read the previous separator char


        // parse path into components using the separator
        // and store into pathcomponents array.
    }
}

