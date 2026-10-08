// clang-tidy plugin with sw_battle_test's project checks. Load it with `clang-tidy --load=<path>`;
// `tools/scripts/check.py tidy` builds and loads it automatically.

#include <clang-tidy/ClangTidyModule.h>
#include <clang/Basic/Version.h>
#include <cstdlib>
#include <llvm/Support/raw_ostream.h>
#include <string>

#include "ConstrainedTemplatesCheck.hpp"
#include "DocumentedApiCheck.hpp"
#include "IncludeStyleCheck.hpp"

namespace sw::tidy
{
	namespace
	{
		class SwTidyModule : public clang::tidy::ClangTidyModule
		{
		public:
			void addCheckFactories(clang::tidy::ClangTidyCheckFactories& factories) override
			{
				factories.registerCheck<IncludeStyleCheck>("sw-include-style");
				factories.registerCheck<ConstrainedTemplatesCheck>("sw-constrained-templates");
				factories.registerCheck<DocumentedApiCheck>("sw-documented-api");
			}
		};

		/// The plugin resolves clang-tidy's symbols at load time, so it only works inside the exact LLVM release it
		/// was compiled against. A mismatch (typically after `brew upgrade llvm`) fails in confusing ways or crashes;
		/// refuse to load instead.
		bool checkLlvmVersion()
		{
			const std::string runtime = clang::getClangFullVersion();
			const std::string expected = " " CLANG_VERSION_STRING;
			const auto at = runtime.find(expected);
			const bool matches = at != std::string::npos
								 && (at + expected.size() == runtime.size() || runtime[at + expected.size()] == ' ');
			if (!matches)
			{
				llvm::errs() << "error: SwTidyChecks was built against LLVM " CLANG_VERSION_STRING
								" but is loaded into '"
							 << runtime
							 << "'. Rebuild it: delete the build directory and rerun `tools/scripts/check.py tidy`.\n";
				std::exit(1);
			}
			return true;
		}

		const bool llvmVersionChecked = checkLlvmVersion();

		const clang::tidy::ClangTidyModuleRegistry::Add<SwTidyModule> registration(
				"sw-module", "sw_battle_test project checks.");
	}
}
