#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace sw::tidy
{
	/// sw-constrained-templates: every template type parameter names a concept.
	///
	///   template <class T> void f(T);                    flagged: unconstrained
	///   template <base::Component C> void f(C);          fine
	///   [](const auto& value) { ... }                    flagged: generic lambda parameter
	///   [](const Component auto& value) { ... }          fine
	///
	/// Concept definitions are exempt: their own parameters cannot be constrained by themselves. A `requires` clause
	/// does not count: the constraint must be visible where the parameter is declared.
	class ConstrainedTemplatesCheck : public clang::tidy::ClangTidyCheck
	{
	public:
		using ClangTidyCheck::ClangTidyCheck;

		void registerMatchers(clang::ast_matchers::MatchFinder* finder) override;

		void check(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

		bool isLanguageVersionSupported(const clang::LangOptions& options) const override;
	};
}
