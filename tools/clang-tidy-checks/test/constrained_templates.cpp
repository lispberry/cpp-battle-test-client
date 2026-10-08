// Fixture for sw-constrained-templates: every line marked EXPECT must be reported, nothing else.
#include <concepts>

template <class T>
concept Small = sizeof(T) <= 8;  // concept definitions are exempt

template <class T>  // EXPECT
T identity(T value)
{
	return value;
}

template <typename T>  // EXPECT
	requires Small<T>
T same(T value)
{
	return value;
}

template <Small T>
T fine(T value)
{
	return value;
}

template <std::integral... Values>
int sum(Values... values)
{
	return (0 + ... + values);
}

int use()
{
	const auto generic = [](const auto& value) { return value; };  // EXPECT
	const auto constrained = [](const std::integral auto& value) { return value; };
	const auto explicitTemplate = []<Small T>(const T& value) { return value; };
	return identity(1) + same(2) + fine(3) + sum(4, 5) + generic(6) + constrained(7) + explicitTemplate(8);
}
