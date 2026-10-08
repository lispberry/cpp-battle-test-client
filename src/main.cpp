#include <App/Application.hpp>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>

int main(int argc, char** argv)
{
	try
	{
		return sw::run(std::span(argv, static_cast<std::size_t>(argc)), std::cout, std::cerr);
	}
	catch (const std::exception& error)
	{
		std::cerr << "Error: " << error.what() << '\n';
	}
	catch (...)
	{
		std::cerr << "Error: unknown failure\n";
	}
	return EXIT_FAILURE;
}
