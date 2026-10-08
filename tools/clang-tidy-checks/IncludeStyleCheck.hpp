#pragma once

#include <clang-tidy/ClangTidyCheck.h>
#include <string>

namespace sw::tidy
{
	/// sw-include-style: project headers are spelled as a full path from src/, in angle brackets.
	///
	///   #include <Core/World/World.hpp>          canonical
	///   #include <Features/Hunter/Hunter.hpp>   canonical
	///   #include <Hunter/Ability.hpp>           only from inside src/Features/Hunter/
	///
	/// Quoted includes, `.` / `..` segments and any other spelling of a file in the tree are rejected. Where the
	/// include resolved, the diagnostic carries a fix-it with the canonical spelling.
	///
	/// Only directives written in files under the source root are checked. The source root is the nearest
	/// ancestor directory of the including file named `SourceDirectoryName` (default: `src`).
	class IncludeStyleCheck : public clang::tidy::ClangTidyCheck
	{
	public:
		IncludeStyleCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context);

		void registerPPCallbacks(
				const clang::SourceManager& sm,
				clang::Preprocessor* pp,
				clang::Preprocessor* moduleExpanderPp) override;

		void storeOptions(clang::tidy::ClangTidyOptions::OptionMap& options) override;

	private:
		std::string _sourceDirectoryName;
	};
}
