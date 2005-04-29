struct ColorByAge : Server {
	void Process(ChangeQueue &Q);
	ColorByAge(Layout *client,Layout *currentLayout) : Server(client,currentLayout) {}
	~ColorByAge() {}
};
