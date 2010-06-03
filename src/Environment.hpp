#ifndef TUNDRA_ENVIRONMENT_HPP
#define TUNDRA_ENVIRONMENT_HPP

namespace tundra
{


class Environment
{
private:
	Environment* const mParent;

	typedef std::vector<std::string> VarType;
	typedef std::map<std::string, VarType> BindingMap;

	BindingMap mBindings;

public:
	Environment();
	~Environment();

public:

};

}


#endif