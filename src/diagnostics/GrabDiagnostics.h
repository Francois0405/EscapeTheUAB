#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace net
{
	enum class GrabDiagnosticRole { Host, Client, Offline };

	// Optional, process-local append sink for the existing grab diagnostic records.
	// Each file is truncated when a process starts and capped at MaxBytesPerRun.
	class GrabDiagnostics
	{
	public:
		static constexpr std::size_t MaxBytesPerRun = 1024 * 1024;

		GrabDiagnostics() = default;
		GrabDiagnostics(const std::filesystem::path& directory, GrabDiagnosticRole role,
			unsigned long processId);

		void Write(const std::string& record);
		const std::filesystem::path& Path() const { return path_; }
		bool Enabled() const { return stream_.is_open(); }

	private:
		std::filesystem::path path_;
		std::ofstream stream_;
		std::size_t bytesWritten_ = 0;
		bool capNoticeWritten_ = false;
	};

	bool GrabPersistenceEnabledFromEnvironment();
	std::filesystem::path DefaultGrabDiagnosticsDirectory();
	unsigned long CurrentProcessId();
	const char* GrabDiagnosticRoleName(GrabDiagnosticRole role);
}
