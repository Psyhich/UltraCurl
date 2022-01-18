#ifndef CLI_PROGRESS_HELPER_H
#define CLI_PROGRESS_HELPER_H

#include <cstddef>
#include <map>

#include "uri.h"

namespace CLI
{
	class CCLIProgressPrinter
	{
	public:
		/// Prints scheme that show which bytes are downloaded and how much data to be downloaded
		void PrintHelp() noexcept;
		/// Prints how much bytes are downloaded and how much about to download
		void PrintProgress(std::multimap<CURI, std::tuple<std::size_t, std::size_t>> dataToShow) noexcept;
		/// Prints enough new lines to print new information
		void Refresh() noexcept;
	protected:
	private:
		static char ReduceUnit(double &dBytesToReduce) noexcept;
		static void PrintBytesCell(double dBytesToPrint, char cUnitToPrint) noexcept;
	private:
		std::size_t m_nLastAddedLines{0};
		std::size_t m_nCycleProgress{0};

		const constexpr static std::size_t CYCLE_LIMIT{5000};
		const constexpr static std::size_t SIZE_OF_URI_CELL{11};
		const constexpr static std::array<char, 5> UNITS{ 'T', 'G', 'M', 'K', 'B' };
		const constexpr static std::array<double, 5> UNITS_BYTES { 1099511627776, 1073741824, 1048576, 1024, 1 };
	};

}


#endif // CLI_PROGRESS_HELPER_H
