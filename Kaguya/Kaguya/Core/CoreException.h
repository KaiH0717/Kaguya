#pragma once
#include <cassert>
#include <stdexcept>

class CoreException : public std::exception
{
public:
	CoreException(const char* File, int Line);

	virtual const char* GetErrorType() const noexcept;
	virtual std::string GetError() const;

	const char* what() const noexcept final;

private:
	std::string GetExceptionOrigin() const;

	std::string GetErrorString() const;

private:
	std::string File;
	int			Line;

	mutable std::string Error;
};
