
class _FConsole {
public:
	_FConsole() : _s(NULL) {};
	void begin(Stream& s);
	void service();
	Stream* _s;
};

extern _FConsole FConsole;
