#ifndef TUNDRA_ACTION_HPP
#define TUNDRA_ACTION_HPP

namespace tundra
{

class Action
{
private:
	Action() {}

public:

	virtual ~Action() = 0;

public:
	int Invoke(Node* n);

private:
	Action(const Action&);
	Action& operator=(const Action&);
};


}

#endif