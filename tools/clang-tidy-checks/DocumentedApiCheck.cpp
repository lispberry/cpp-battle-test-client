#include "DocumentedApiCheck.hpp"

#include <algorithm>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RawCommentList.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <optional>

namespace sw::tidy
{
	using namespace clang::ast_matchers;

	namespace
	{
		/// The file lies under a directory named `Core`: only the engine's API must be documented.
		bool inCore(llvm::StringRef file)
		{
			return std::any_of(
					llvm::sys::path::begin(file),
					llvm::sys::path::end(file),
					[](llvm::StringRef component) { return component == "Core"; });
		}

		bool inHiddenNamespace(const clang::Decl* decl)
		{
			for (const clang::DeclContext* context = decl->getDeclContext(); context != nullptr;
				 context = context->getParent())
			{
				if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(context))
				{
					if (ns->isAnonymousNamespace() || ns->getName() == "detail")
					{
						return true;
					}
				}
			}
			return false;
		}

		/// Public all the way out: the declaration and every class around it.
		bool isPubliclyAccessible(const clang::Decl* decl)
		{
			for (const clang::Decl* current = decl; current != nullptr;
				 current = llvm::dyn_cast_or_null<clang::Decl>(current->getDeclContext()))
			{
				if (!llvm::isa<clang::CXXRecordDecl>(current->getDeclContext()))
				{
					return true;
				}
				const clang::AccessSpecifier access = current->getAccess();
				if (access != clang::AS_public && access != clang::AS_none)
				{
					return false;
				}
			}
			return true;
		}

		/// Functions that need no comment of their own.
		bool isExemptFunction(const clang::FunctionDecl* function)
		{
			if (function->isDeleted() || function->isExplicitlyDefaulted() || function->isOverloadedOperator()
				|| llvm::isa<clang::CXXDestructorDecl>(function) || llvm::isa<clang::CXXConversionDecl>(function)
				|| llvm::isa<clang::CXXDeductionGuideDecl>(function))
			{
				return true;
			}
			if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(function))
			{
				return constructor->isCopyOrMoveConstructor();
			}
			if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(function))
			{
				return method->size_overridden_methods() > 0 || method->hasAttr<clang::OverrideAttr>();
			}
			return false;
		}

		bool isPublicApi(const clang::NamedDecl* decl)
		{
			if (decl->isImplicit() || decl->isInvalidDecl() || decl->getDeclName().isEmpty()
				|| decl->getParentFunctionOrMethod() != nullptr || inHiddenNamespace(decl)
				|| !isPubliclyAccessible(decl))
			{
				return false;
			}
			if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl))
			{
				return function->isFirstDecl() && !function->isTemplateInstantiation()
					   && !function->getPrimaryTemplate() && !isExemptFunction(function);
			}
			if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(decl))
			{
				return !record->isLambda() && !llvm::isa<clang::ClassTemplateSpecializationDecl>(record)
					   && !record->isAnonymousStructOrUnion() && !record->getName().empty();
			}
			if (const auto* alias = llvm::dyn_cast<clang::TypedefNameDecl>(decl))
			{
				return alias->getDeclContext()->isFileContext();
			}
			if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(decl))
			{
				return variable->isFirstDecl()
					   && (variable->getDeclContext()->isFileContext() || variable->isStaticDataMember())
					   && !llvm::isa<clang::VarTemplateSpecializationDecl>(variable);
			}
			return llvm::isa<clang::ConceptDecl>(decl);
		}

		/// Another overload of the same name in the same scope is documented: one comment covers an overload set.
		bool hasDocumentedOverload(const clang::NamedDecl* decl, const clang::ASTContext& context)
		{
			const auto* function = llvm::dyn_cast<clang::FunctionDecl>(decl);
			if (function == nullptr || !decl->getDeclName().isIdentifier())
			{
				return false;
			}
			for (const clang::NamedDecl* sibling : decl->getDeclContext()->lookup(decl->getDeclName()))
			{
				if (sibling != decl && context.getRawCommentForAnyRedecl(sibling) != nullptr)
				{
					return true;
				}
			}
			return false;
		}

		/// Concepts and class templates must show how they are used: their requirements are the hardest part to
		/// guess from the signatures. Everything else needs only a sentence.
		bool needsExample(const clang::NamedDecl* decl)
		{
			if (llvm::isa<clang::ConceptDecl>(decl))
			{
				return true;
			}
			const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
			return record != nullptr && record->getDescribedClassTemplate() != nullptr;
		}

		/// A member of a documented class: the class comment covers its methods, nested types and static members.
		bool inDocumentedClass(const clang::NamedDecl* decl, const clang::ASTContext& context)
		{
			for (const clang::DeclContext* scope = decl->getDeclContext(); scope != nullptr; scope = scope->getParent())
			{
				const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(scope);
				if (record != nullptr && context.getRawCommentForAnyRedecl(record) != nullptr)
				{
					return true;
				}
			}
			return false;
		}

		bool hasExample(llvm::StringRef comment)
		{
			return comment.contains("\\code") || comment.contains("@code");
		}

		const char* kindOf(const clang::NamedDecl* decl)
		{
			if (llvm::isa<clang::ConceptDecl>(decl))
			{
				return "concept";
			}
			if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(decl))
			{
				return record->getDescribedClassTemplate() != nullptr ? "class template" : "class";
			}
			if (llvm::isa<clang::FunctionDecl>(decl))
			{
				return "function";
			}
			if (llvm::isa<clang::TypedefNameDecl>(decl))
			{
				return "type alias";
			}
			return "variable";
		}
	}

	DocumentedApiCheck::DocumentedApiCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context) :
			ClangTidyCheck(name, context)
	{}

	void DocumentedApiCheck::registerMatchers(MatchFinder* finder)
	{
		finder->addMatcher(
				namedDecl(anyOf(cxxRecordDecl(isDefinition()),
								functionDecl(),
								conceptDecl(),
								typedefNameDecl(),
								varDecl(unless(parmVarDecl()))))
						.bind("declaration"),
				this);
	}

	void DocumentedApiCheck::check(const MatchFinder::MatchResult& result)
	{
		const auto* decl = result.Nodes.getNodeAs<clang::NamedDecl>("declaration");
		const clang::SourceManager& sources = *result.SourceManager;
		const clang::SourceLocation location = decl->getLocation();
		if (location.isInvalid() || !location.isFileID() || sources.isInSystemHeader(location)
			|| !inCore(sources.getFilename(location)) || !isPublicApi(decl))
		{
			return;
		}
		const clang::RawComment* comment = result.Context->getRawCommentForAnyRedecl(decl);
		if (comment == nullptr
			&& (inDocumentedClass(decl, *result.Context) || hasDocumentedOverload(decl, *result.Context)))
		{
			return;
		}
		if (comment == nullptr)
		{
			diag(location, "%0 %1 is public API and needs a documentation comment (///)") << kindOf(decl) << decl;
			return;
		}
		if (needsExample(decl) && !hasExample(comment->getRawText(sources)))
		{
			diag(location, "%0 %1 needs an example in its documentation (\\code ... \\endcode)")
					<< kindOf(decl) << decl;
		}
	}

	bool DocumentedApiCheck::isLanguageVersionSupported(const clang::LangOptions& options) const
	{
		return options.CPlusPlus;
	}

	std::optional<clang::TraversalKind> DocumentedApiCheck::getCheckTraversalKind() const
	{
		// Only what is written in the source: no template instantiations, no implicit members.
		return clang::TK_IgnoreUnlessSpelledInSource;
	}
}
