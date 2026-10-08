#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace sw::tidy
{
	/// sw-documented-api: the public API of the engine has documentation comments, so that it can be explored
	/// (in the editor and in the Doxygen output) without reading the implementation.
	///
	/// Checked only in files under a directory named `Core`, the engine (never in Features):
	///   - classes and structs, concepts, functions and methods, namespace-scope type aliases and variables, public
	///     static data members;
	///   - public only: private and protected members, `detail` and anonymous namespaces are skipped;
	///   - not required: enums, overrides (documented on the base), defaulted or deleted functions, copy and move
	///     constructors, destructors, operators, template instantiations and specializations.
	///
	/// One comment covers more than one declaration:
	///   - a class comment covers the class's members (methods, nested types, static members): document a member only
	///     when it has a rule of its own to state;
	///   - an overload is accepted when another one of the same name is documented.
	/// The comment must be a documentation comment (`///` or `/** */`). Concepts and class templates must also show an
	/// example (`\code ... \endcode`): their requirements are the hardest part to guess from the signatures.
	class DocumentedApiCheck : public clang::tidy::ClangTidyCheck
	{
	public:
		DocumentedApiCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context);

		void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;

		void check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

		bool isLanguageVersionSupported(const clang::LangOptions& options) const override;

		std::optional<clang::TraversalKind> getCheckTraversalKind() const override;
	};
}
