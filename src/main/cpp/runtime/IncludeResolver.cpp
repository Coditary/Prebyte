#include "runtime/IncludeResolver.h"

#include "support/Diagnostic.h"

#include <algorithm>

namespace prebyte {

namespace {

Diagnostic make_include_error(const std::string& message, const std::filesystem::path& path,
                              const RenderSession& session) {
    Diagnostic diagnostic;
    diagnostic.code = "RUNTIME002";
    diagnostic.message = message;
    diagnostic.span.file_path = path.string();
    for (const auto& include : session.include_stack) {
        diagnostic.include_chain.push_back(include.string());
    }
    return diagnostic;
}

bool try_load_candidate(const std::filesystem::path& candidate, RenderSession& session, ResolvedInclude& resolved) {
    InputBuffer source;
    try {
        source = InputBuffer::from_file(candidate);
    } catch (const std::exception&) {
        return false;
    }

    const std::filesystem::path absolute = std::filesystem::absolute(candidate);
    if (std::find(session.include_stack.begin(), session.include_stack.end(), absolute) != session.include_stack.end()) {
        throw DiagnosticError(make_include_error("Include cycle detected", absolute, session));
    }

    session.include_stack.push_back(absolute);
    resolved = ResolvedInclude{absolute, std::move(source)};
    return true;
}

}

ResolvedInclude IncludeResolver::load(const std::string& include_path, const std::filesystem::path& current_file,
                                      const EffectiveSettings& settings, RenderSession& session) const {
    ResolvedInclude resolved;
    if (!current_file.empty()) {
        if (try_load_candidate(current_file.parent_path() / include_path, session, resolved)) {
            return resolved;
        }
    }
    if (!settings.include_path.empty()) {
        if (try_load_candidate(settings.include_path / include_path, session, resolved)) {
            return resolved;
        }
    }
    if (try_load_candidate(include_path, session, resolved)) {
        return resolved;
    }

    throw DiagnosticError(make_include_error("Include not found: " + include_path, include_path, session));
}

void IncludeResolver::pop(RenderSession& session) const {
    if (!session.include_stack.empty()) {
        session.include_stack.pop_back();
    }
}

}
