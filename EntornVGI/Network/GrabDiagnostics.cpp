#include "GrabDiagnostics.h"

#include <cstdlib>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace net
{
	namespace
	{
		bool EnabledValue(const char* name)
		{
			const char* value = std::getenv(name);
			return value != nullptr && value[0] != '\0' && value[0] != '0';
		}
	}

	const char* GrabDiagnosticRoleName(GrabDiagnosticRole role)
	{
		switch (role)
		{
		case GrabDiagnosticRole::Host: return "host";
		case GrabDiagnosticRole::Client: return "client";
		default: return "offline";
		}
	}

	GrabDiagnostics::GrabDiagnostics(const std::filesystem::path& directory,
		GrabDiagnosticRole role, unsigned long processId)
	{
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error) return;

		std::ostringstream filename;
		filename << "grab-" << GrabDiagnosticRoleName(role) << '-' << processId << ".log";
		path_ = directory / filename.str();
		stream_.open(path_, std::ios::binary | std::ios::trunc);
		if (!stream_) path_.clear();
	}

	void GrabDiagnostics::Write(const std::string& record)
	{
		if (!stream_.is_open() || capNoticeWritten_) return;
		const std::size_t remaining = MaxBytesPerRun - bytesWritten_;
		if (record.size() + 1 <= remaining)
		{
			stream_ << record << '\n';
			stream_.flush();
			if (stream_) bytesWritten_ += record.size() + 1;
			return;
		}

		static const std::string notice = "[grab] persistence stopped: 1 MiB per-process limit reached\n";
		if (remaining >= notice.size())
		{
			stream_ << notice;
			stream_.flush();
			bytesWritten_ += notice.size();
		}
		capNoticeWritten_ = true;
	}

	bool GrabPersistenceEnabledFromEnvironment()
	{
		return EnabledValue("ENTORNVGI_DEBUG_GRAB") && EnabledValue("ENTORNVGI_DEBUG_GRAB_PERSIST");
	}

	std::filesystem::path DefaultGrabDiagnosticsDirectory()
	{
#ifdef _WIN32
		const char* base = std::getenv("LOCALAPPDATA");
		if (base != nullptr && base[0] != '\0')
			return std::filesystem::path(base) / "EscapeTheUAB" / "logs";
#elif defined(__APPLE__)
		const char* home = std::getenv("HOME");
		if (home != nullptr && home[0] != '\0')
			return std::filesystem::path(home) / "Library" / "Logs" / "EscapeTheUAB";
#else
		const char* config = std::getenv("XDG_STATE_HOME");
		if (config != nullptr && config[0] != '\0')
			return std::filesystem::path(config) / "EscapeTheUAB" / "logs";
		const char* home = std::getenv("HOME");
		if (home != nullptr && home[0] != '\0')
			return std::filesystem::path(home) / ".local" / "state" / "EscapeTheUAB" / "logs";
#endif
		return {};
	}

	unsigned long CurrentProcessId()
	{
#ifdef _WIN32
		return static_cast<unsigned long>(::GetCurrentProcessId());
#else
		return static_cast<unsigned long>(::getpid());
#endif
	}
}
