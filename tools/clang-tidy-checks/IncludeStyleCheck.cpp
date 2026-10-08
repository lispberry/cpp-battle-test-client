#include "IncludeStyleCheck.hpp"

#include <algorithm>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sw::tidy
{
	namespace
	{
		using clang::CharSourceRange;
		using clang::SourceLocation;
		using llvm::StringRef;

		constexpr StringRef CoreDirectory = "Core";
		constexpr StringRef FeaturesDirectory = "Features";
		constexpr StringRef AppDirectory = "App";

		/// Nearest ancestor directory of `file` named `name`, or empty.
		std::string findSourceRoot(StringRef file, StringRef name)
		{
			for (StringRef dir = llvm::sys::path::parent_path(file); !dir.empty();
				 dir = llvm::sys::path::parent_path(dir))
			{
				if (llvm::sys::path::filename(dir) == name)
				{
					return dir.str();
				}
			}
			return {};
		}

		/// `path` relative to `root` with forward slashes, if it lies under `root`.
		std::optional<std::string> relativeTo(StringRef path, StringRef root)
		{
			if (root.empty() || !path.starts_with(root))
			{
				return std::nullopt;
			}
			StringRef rest = path.drop_front(root.size());
			if (rest.empty() || !llvm::sys::path::is_separator(rest.front()))
			{
				return std::nullopt;
			}
			return llvm::sys::path::convert_to_slash(rest.drop_front());
		}

		bool isProjectPath(StringRef relative)
		{
			const StringRef first = relative.split('/').first;
			return first == CoreDirectory || first == FeaturesDirectory || first == AppDirectory;
		}

		/// The convention, spelled out in every warning that breaks it. The short-form exception is only mentioned
		/// to files inside a feature, the only place it applies.
		std::string rule(StringRef includer)
		{
			std::string text
					= "project headers are included with angle brackets and their full path from src/, "
					  "e.g. #include <Core/World/World.hpp>";
			const auto [first, rest] = includer.split('/');
			const auto [feature, inside] = rest.split('/');
			if (first == FeaturesDirectory && !inside.empty())
			{
				text += "; inside src/Features/" + feature.str() + "/ this feature's own headers may also use the "
						"short form #include <" + feature.str() + "/...>";
			}
			return text;
		}

		/// What is wrong with one #include and how to fix it. Rendered as a warning followed by notes.
		struct Finding
		{
			std::string problem;
			std::vector<std::string> explanation;
			/// Full path from src/ that the include must be spelled as. Carries a fix-it.
			std::optional<std::string> replacement;
			/// Best guess when the include did not resolve. Only suggested, never applied.
			std::optional<std::string> guess;
			/// Attach the fix-it to the "write this instead" note instead of the warning. clang-tidy drops fix-its on
			/// the same range from both diagnostics when the compiler attaches its own to an error, but leaves note
			/// fix-its alone; `check.py tidy --fix` then discards the compiler's and applies this one.
			bool fixOnNote = false;
		};

		class IncludeStyleCallbacks : public clang::PPCallbacks
		{
		public:
			IncludeStyleCallbacks(
					IncludeStyleCheck& check, const clang::SourceManager& sm, StringRef sourceDirectoryName) :
					_check(check),
					_sm(sm),
					_sourceDirectoryName(sourceDirectoryName.str())
			{}

			void InclusionDirective(
					SourceLocation hashLoc,
					const clang::Token& /*includeTok*/,
					StringRef spelling,
					bool isAngled,
					CharSourceRange filenameRange,
					clang::OptionalFileEntryRef file,
					StringRef searchPath,
					StringRef /*relativePath*/,
					const clang::Module* /*suggestedModule*/,
					bool /*moduleImported*/,
					clang::SrcMgr::CharacteristicKind /*fileType*/) override
			{
				const clang::OptionalFileEntryRef includer = _sm.getFileEntryRefForID(_sm.getFileID(hashLoc));
				if (!includer)
				{
					return;
				}
				clang::FileManager& files = _sm.getFileManager();
				const StringRef includerPath = files.getCanonicalName(*includer);
				const std::string root = findSourceRoot(includerPath, _sourceDirectoryName);
				const std::optional<std::string> includerRelative = relativeTo(includerPath, root);
				if (!includerRelative)
				{
					return;
				}

				const Directive directive{
						.range = filenameRange,
						.written = "#include " + (isAngled ? "<" + spelling.str() + ">" : "\"" + spelling.str() + "\""),
						.root = root,
						.includer = *includerRelative};
				const std::optional<std::string> resolved
						= file ? relativeTo(files.getCanonicalName(*file), root) : std::nullopt;

				if (!isAngled)
				{
					emit(directive, quotedInclude(directive, spelling, resolved, file.has_value()));
					return;
				}

				llvm::SmallVector<StringRef, 8> segments;
				spelling.split(segments, '/');
				const auto dotSegment = std::ranges::find_if(
						segments, [](StringRef segment) { return segment == "." || segment == ".."; });
				if (dotSegment != segments.end())
				{
					emit(directive,
						 {.problem = directive.written + " contains a '" + dotSegment->str() + "' segment",
						  .explanation
						  = {"'.' and '..' make the same header reachable under many spellings, so the path from src/ "
							 "is the only one allowed",
							 rule(directive.includer)},
						  .replacement = resolved});
					return;
				}

				if (resolved)
				{
					// An angled include that clang only found next to the including file: it reports an error
					// itself, with a fix-it to switch to quotes, which is just as wrong.
					const bool foundNextToIncluder = samePath(searchPath, llvm::sys::path::parent_path(includerPath));
					checkResolved(directive, spelling, segments.front(), *resolved, foundNextToIncluder);
					return;
				}
				if (file)
				{
					return;	 // standard or third-party header
				}

				// Unresolved: clang reports "file not found" itself. Explain what the spelling should have been
				// when it names a project header.
				if (segments.front() == CoreDirectory || segments.front() == FeaturesDirectory
					|| segments.front() == AppDirectory)
				{
					emit(directive,
						 {.problem = directive.written + ": there is no src/" + spelling.str(),
						  .guess = findByBasename(root, spelling)});
				}
				else if (segments.size() > 1 && treeHasDirectory(root, segments.front()))
				{
					emit(directive,
						 {.problem = directive.written + " is not a path from src/",
						  .explanation = {rule(directive.includer)},
						  .guess = findByBasename(root, spelling)});
				}
			}

		private:
			struct Directive
			{
				CharSourceRange range;
				std::string written;   // the directive as written, e.g. #include "Unit.hpp"
				std::string root;	   // absolute path of src/
				std::string includer;  // including file, relative to src/
			};

			Finding quotedInclude(
					const Directive& directive,
					StringRef spelling,
					const std::optional<std::string>& resolved,
					bool found)
			{
				Finding finding{
						.problem = directive.written + " uses quotes",
						.explanation
						= {"a quoted include is looked up next to the including file first, so the same line can "
						   "name a different header depending on where it is written, and moving a file silently "
						   "changes what it includes"}};
				if (resolved)
				{
					finding.explanation.push_back(rule(directive.includer));
					finding.replacement = resolved;
				}
				else if (found)
				{
					finding.explanation.emplace_back("standard and third-party headers use angle brackets too");
					finding.replacement = spelling.str();
				}
				else
				{
					finding.explanation.push_back(rule(directive.includer));
					finding.guess = findByBasename(directive.root, spelling);
				}
				return finding;
			}

			/// An angle-bracketed include that resolved to `resolved` (relative to src/).
			void checkResolved(
					const Directive& directive,
					StringRef spelling,
					StringRef first,
					StringRef resolved,
					bool foundNextToIncluder)
			{
				if (!isProjectPath(resolved))
				{
					emit(directive,
						 {.problem = directive.written + " refers to src/" + resolved.str()
									 + ", which is outside src/Core/, src/Features/ and src/App/",
						  .explanation = {"project headers live in src/Core/ (the engine), src/Features/ (per-unit "
										  "mechanics) or src/App/ (the program); move the header into one of them"}});
					return;
				}
				if (spelling == resolved)
				{
					return;
				}

				const std::string featuresForm = (FeaturesDirectory + "/" + spelling).str();
				if (featuresForm == resolved)
				{
					const std::string featureDirectory = (FeaturesDirectory + "/" + first + "/").str();
					if (StringRef(directive.includer).starts_with(featureDirectory))
					{
						return;	 // the short form, inside its own feature
					}
					emit(directive,
						 {.problem = directive.written + " uses the short form for a header of another feature",
						  .explanation
						  = {"the short form #include <" + first.str() + "/...> is only allowed inside src/"
							 + featureDirectory + "; " + directive.includer
							 + " is outside it, so it spells the full path from src/"},
						  .replacement = featuresForm});
					return;
				}

				Finding finding{
						.problem
						= directive.written + " does not spell the full path from src/ to src/" + resolved.str(),
						.explanation = {rule(directive.includer)},
						.replacement = resolved.str(),
						.fixOnNote = foundNextToIncluder};
				if (foundNextToIncluder)
				{
					finding.explanation.emplace_back(
							"ignore the compiler's suggestion to use quotes: quoted includes are not allowed either "
							"(`uv run tools/scripts/check.py tidy --fix` applies the right fix)");
				}
				emit(directive, finding);
			}

			/// Warning at the include, then the explanation as notes, then what to write instead. Explanatory notes
			/// carry no location, so they print as plain text instead of repeating the source line.
			void emit(const Directive& directive, const Finding& finding)
			{
				const SourceLocation loc = directive.range.getBegin();
				const bool fixable = finding.replacement && directive.range.isValid() && !loc.isMacroID();
				{
					auto warning = _check.diag(loc, "%0") << finding.problem;
					if (fixable && !finding.fixOnNote)
					{
						warning << clang::FixItHint::CreateReplacement(
								directive.range, "<" + *finding.replacement + ">");
					}
				}  // the warning is emitted here, before its notes

				for (const std::string& line : finding.explanation)
				{
					_check.diag(SourceLocation(), "%0", clang::DiagnosticIDs::Note) << line;
				}

				if (finding.replacement)
				{
					if (fixable && finding.fixOnNote)
					{
						_check.diag(loc, "write #include <%0> instead", clang::DiagnosticIDs::Note)
								<< *finding.replacement
								<< clang::FixItHint::CreateReplacement(
										   directive.range, "<" + *finding.replacement + ">");
					}
					else
					{
						_check.diag(SourceLocation(), "write #include <%0> instead", clang::DiagnosticIDs::Note)
								<< *finding.replacement;
					}
				}
				else if (finding.guess)
				{
					_check.diag(SourceLocation(), "did you mean #include <%0>?", clang::DiagnosticIDs::Note)
							<< *finding.guess;
				}
			}

			static bool samePath(StringRef a, StringRef b)
			{
				llvm::SmallString<256> realA;
				llvm::SmallString<256> realB;
				return !a.empty() && !llvm::sys::fs::real_path(a, realA) && !llvm::sys::fs::real_path(b, realB)
					   && realA == realB;
			}

			/// Full path from the source root of the first file sharing `spelling`'s basename.
			std::optional<std::string> findByBasename(const std::string& root, StringRef spelling)
			{
				const StringRef name = llvm::sys::path::filename(spelling, llvm::sys::path::Style::posix);
				const std::string key = root + '\0' + name.str();
				if (auto cached = _basenameCache.find(key); cached != _basenameCache.end())
				{
					return cached->second;
				}

				std::vector<std::string> matches;
				std::error_code error;
				for (llvm::sys::fs::recursive_directory_iterator it(root, error), end; it != end && !error;
					 it.increment(error))
				{
					if (it->type() == llvm::sys::fs::file_type::regular_file
						&& llvm::sys::path::filename(it->path()) == name)
					{
						if (std::optional<std::string> relative = relativeTo(it->path(), root))
						{
							matches.push_back(std::move(*relative));
						}
					}
				}
				std::ranges::sort(matches);

				std::optional<std::string> result;
				if (!matches.empty())
				{
					result = matches.front();
				}
				_basenameCache[key] = result;
				return result;
			}

			bool treeHasDirectory(const std::string& root, StringRef name)
			{
				std::error_code error;
				for (llvm::sys::fs::recursive_directory_iterator it(root, error), end; it != end && !error;
					 it.increment(error))
				{
					if (it->type() == llvm::sys::fs::file_type::directory_file
						&& llvm::sys::path::filename(it->path()) == name)
					{
						return true;
					}
				}
				return false;
			}

			IncludeStyleCheck& _check;
			const clang::SourceManager& _sm;
			std::string _sourceDirectoryName;
			llvm::StringMap<std::optional<std::string>> _basenameCache;
		};
	}

	IncludeStyleCheck::IncludeStyleCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context) :
			ClangTidyCheck(name, context),
			_sourceDirectoryName(Options.get("SourceDirectoryName", "src"))
	{}

	void IncludeStyleCheck::registerPPCallbacks(
			const clang::SourceManager& sm, clang::Preprocessor* pp, clang::Preprocessor* /*moduleExpanderPp*/)
	{
		pp->addPPCallbacks(std::make_unique<IncludeStyleCallbacks>(*this, sm, _sourceDirectoryName));
	}

	void IncludeStyleCheck::storeOptions(clang::tidy::ClangTidyOptions::OptionMap& options)
	{
		Options.store(options, "SourceDirectoryName", _sourceDirectoryName);
	}
}
