// Fixture for sw-documented-api (it lives under a Core/ directory, like the engine): every line marked EXPECT must be
// reported, nothing else. Rule: public declarations need `///`, except members of a documented class; concepts and
// class templates also need a \code example.
#include <concepts>

class Undocumented  // EXPECT
{
public:
	void run();  // EXPECT

	/// Documented.
	void stop();

	int publicField = 0;  // fields are described by their class

	Undocumented() = default;
	Undocumented(const Undocumented&) = default;
	~Undocumented();

	bool operator==(const Undocumented&) const = default;

	static constexpr int Limit = 3;  // EXPECT

private:
	void hidden();
};

/// A documented class: its comment covers the members, and a plain class needs no example.
struct Documented
{
	/// Documented: a member may still state a rule of its own.
	void act() const;

	void go();  // covered by the class comment

	static constexpr int Max = 1;  // covered by the class comment

	struct Nested  // covered by the class comment, and so are its members
	{
		void inner();
	};

	/// Documented: overriding it needs nothing more.
	virtual void hook();
	virtual ~Documented() = default;
};

/// Overrides are documented on the base, and an override is not a new method to show.
struct Derived final : Documented
{
	void hook() override;
};

/// Plain data needs only a sentence.
struct Point
{
	int x = 0;
	int y = 0;

	bool operator==(const Point&) const = default;
};

/// A class with methods and an example.
///
/// \code
/// Counter counter;
/// counter.add();
/// \endcode
class Counter
{
public:
	/// Counts one more, or `n` more.
	void add();
	void add(int n);

private:
	int _count = 0;
};

/// Unnamed parameters of function types are not declarations to document.
using Callback = void (*)(int, const char*);

enum class Kind  // enums are exempt
{
	One,
};

template <class T>
concept Sized = sizeof(T) > 0;  // EXPECT

/// A concept without an example.
template <class T>
concept Small = sizeof(T) <= 8;  // EXPECT

/// A concept with an example.
///
/// \code
/// static_assert(Tiny<char>);
/// \endcode
template <class T>
concept Tiny = sizeof(T) == 1;

/// A class template without an example.
template <Tiny T>
class Box  // EXPECT
{
public:
	/// Documented.
	T get() const;
};

/// A class template with an example.
///
/// \code
/// Crate<char> crate;
/// \endcode
template <Tiny T>
class Crate
{};

using Number = int;  // EXPECT

/// Documented alias.
using Text = const char*;

int answer();  // EXPECT

/// Documented, so its definition below needs nothing.
int question();

int question()
{
	return answer();
}

inline constexpr int Ready = 1;  // EXPECT

namespace detail
{
	int internal();
}

namespace
{
	int local()
	{
		return 0;
	}
}

// A plain comment is not documentation.
int plain();  // EXPECT
