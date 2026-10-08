#include "ConstrainedTemplatesCheck.hpp"

#include <clang/AST/DeclTemplate.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>

namespace sw::tidy
{
	using namespace clang::ast_matchers;

	void ConstrainedTemplatesCheck::registerMatchers(MatchFinder* finder)
	{
		finder->addMatcher(templateTypeParmDecl(unless(hasAncestor(conceptDecl()))).bind("parameter"), this);
	}

	void ConstrainedTemplatesCheck::check(const MatchFinder::MatchResult& result)
	{
		const auto* parameter = result.Nodes.getNodeAs<clang::TemplateTypeParmDecl>("parameter");
		if (parameter == nullptr || parameter->hasTypeConstraint())
		{
			return;
		}
		const clang::SourceLocation location = parameter->getLocation();
		if (location.isInvalid() || result.SourceManager->isInSystemHeader(location))
		{
			return;
		}
		if (parameter->isImplicit())
		{
			diag(location, "generic lambda parameter is not constrained; write `Concept auto` instead of `auto`");
			return;
		}
		diag(location, "template parameter %0 is not constrained; name a concept instead of `class`/`typename`")
				<< parameter;
	}

	bool ConstrainedTemplatesCheck::isLanguageVersionSupported(const clang::LangOptions& options) const
	{
		return options.CPlusPlus20;
	}
}
