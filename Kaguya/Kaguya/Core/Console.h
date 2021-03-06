#pragma once
#include <string_view>

class IConsoleVariable
{
public:
	virtual bool		GetBool() const	  = 0;
	virtual int			GetInt() const	  = 0;
	virtual float		GetFloat() const  = 0;
	virtual std::string GetString() const = 0;

	virtual void Set(std::string_view Value) = 0;

	void Set(bool Value) { Set(Value ? "true" : "false"); }
	void Set(int Value)
	{
		std::string s = std::to_string(Value);
		Set(s.data());
	}
	void Set(float Value)
	{
		std::string s = std::to_string(Value);
		Set(s.data());
	}
};

template<typename T>
class AutoConsoleVariable
{
public:
	AutoConsoleVariable(IConsoleVariable* ConsoleVariable)
		: ConsoleVariable(ConsoleVariable)
	{
	}
	AutoConsoleVariable(std::string_view Name, std::string_view Description, const T& DefaultValue) {}

	operator T();

	IConsoleVariable&		operator*() { return *ConsoleVariable; }
	const IConsoleVariable& operator*() const { return *ConsoleVariable; }
	IConsoleVariable*		operator->() { return ConsoleVariable; }
	const IConsoleVariable* operator->() const { return ConsoleVariable; }

private:
	IConsoleVariable* ConsoleVariable;
};

class IConsole
{
public:
	static IConsole& Instance();

	virtual IConsoleVariable* RegisterVariable(
		std::string_view Name,
		std::string_view Description,
		const bool&		 DefaultValue) = 0;

	virtual IConsoleVariable* RegisterVariable(
		std::string_view Name,
		std::string_view Description,
		const int&		 DefaultValue) = 0;

	virtual IConsoleVariable* RegisterVariable(
		std::string_view Name,
		std::string_view Description,
		const float&	 DefaultValue) = 0;

	virtual IConsoleVariable* RegisterVariable(
		std::string_view   Name,
		std::string_view   Description,
		const std::string& DefaultValue) = 0;

	virtual AutoConsoleVariable<bool>		 FindBoolCVar(std::string_view Name)   = 0;
	virtual AutoConsoleVariable<int>		 FindIntCVar(std::string_view Name)	   = 0;
	virtual AutoConsoleVariable<float>		 FindFloatCVar(std::string_view Name)  = 0;
	virtual AutoConsoleVariable<std::string> FindStringCVar(std::string_view Name) = 0;
};

template<>
inline AutoConsoleVariable<bool>::AutoConsoleVariable(
	std::string_view Name,
	std::string_view Description,
	const bool&		 DefaultValue)
	: ConsoleVariable(IConsole::Instance().RegisterVariable(Name, Description, DefaultValue))
{
}
template<>
inline AutoConsoleVariable<bool>::operator bool()
{
	return ConsoleVariable->GetBool();
}

template<>
inline AutoConsoleVariable<int>::AutoConsoleVariable(
	std::string_view Name,
	std::string_view Description,
	const int&		 DefaultValue)
	: ConsoleVariable(IConsole::Instance().RegisterVariable(Name, Description, DefaultValue))
{
}
template<>
inline AutoConsoleVariable<int>::operator int()
{
	return ConsoleVariable->GetInt();
}

template<>
inline AutoConsoleVariable<float>::AutoConsoleVariable(
	std::string_view Name,
	std::string_view Description,
	const float&	 DefaultValue)
	: ConsoleVariable(IConsole::Instance().RegisterVariable(Name, Description, DefaultValue))
{
}
template<>
inline AutoConsoleVariable<float>::operator float()
{
	return ConsoleVariable->GetFloat();
}

template<>
inline AutoConsoleVariable<std::string>::AutoConsoleVariable(
	std::string_view   Name,
	std::string_view   Description,
	const std::string& DefaultValue)
	: ConsoleVariable(IConsole::Instance().RegisterVariable(Name, Description, DefaultValue))
{
}
template<>
inline AutoConsoleVariable<std::string>::operator std::string()
{
	return ConsoleVariable->GetString();
}
