#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <cmath>

#include <algorithm>

#include "cli_progres.h"

char CLI::CCLIProgressPrinter::ReduceUnit(double &dBytesToReduce) noexcept
{
	int iUnitIndex = 0;
	while(dBytesToReduce / UNITS_BYTES[iUnitIndex] < 1)
	{
		++iUnitIndex;
	}

	dBytesToReduce /= UNITS_BYTES[iUnitIndex];
	return UNITS[iUnitIndex];
}

void CLI::CCLIProgressPrinter::PrintBytesCell(double dBytesToPrint, char cUnitToPrint) noexcept
{
	if(std::isnan(dBytesToPrint))
	{
		printf("NON DEF |");
		return;
	}
	// If we have bigger unit than simple bytes we can print with 2 numbers precision
	// Adding spaces to align double value with cell
	for(int i = 100; i > 0 ; i /= 10)
	{
		if(dBytesToPrint / i < 10)
		{
			printf(" ");
		}
	}
	if(cUnitToPrint != 'B')
	{
		printf("%0.2lf", dBytesToPrint);
	}
	else
	{
		printf("   %0.0lf", dBytesToPrint);
	}

	printf("%c|", cUnitToPrint);

}

void CLI::CCLIProgressPrinter::PrintHelp() noexcept
{
	m_nLastAddedLines += 2;
	printf("|    URI    |  Down  | Overal |\n");
}

void CLI::CCLIProgressPrinter::PrintProgress(
	std::multimap<CURI, std::tuple<std::size_t, std::size_t>> dataToShow) noexcept
{
	for(auto &[uri, progress] : dataToShow)
	{
		++m_nLastAddedLines;
		std::string adressWithPath;
		if(auto pureAdress = uri.GetPureAddress())
		{
			adressWithPath = std::move(*pureAdress);
		}
		if(auto path = uri.GetPath())
		{
			adressWithPath += *path;
		}

		// Adding space in the end to split value from itself when we shifted it fully
		adressWithPath.push_back(' ');
		// Shifting addres
		// Calculating overflow, because for all addresses I use one shift value
		auto addressIter = adressWithPath.begin();
		std::advance(addressIter, m_nCycleProgress % adressWithPath.size());
		std::rotate(adressWithPath.begin(), addressIter, adressWithPath.end());

		// Printing address to fit to SIZE_OF_URI_CELL
		putchar('|');
		for(size_t nIndex = 0; nIndex < SIZE_OF_URI_CELL; nIndex++)
		{
			if(nIndex < adressWithPath.size())
			{
				putchar(adressWithPath[nIndex]);
			}
			else
			{
				putchar(' ');
			}
		}
		putchar('|');

		// Transforming bytes count into kilo, mega, giga, or petabytes representation
		double dBytes = std::get<0>(progress);
		char cUnit = ReduceUnit(dBytes);
		PrintBytesCell(dBytes, cUnit);
		
		dBytes = std::get<1>(progress);
		cUnit = ReduceUnit(dBytes);
		PrintBytesCell(dBytes, cUnit);
		putchar('\n');
	}

}
void CLI::CCLIProgressPrinter::Refresh() noexcept
{
	struct winsize terminal;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal);

	for(std::size_t i = 0; i < (terminal.ws_row - m_nLastAddedLines); i++)
	{
		printf("\n");
	}
	m_nLastAddedLines = 0;

	++m_nCycleProgress;
	if(m_nCycleProgress == CYCLE_LIMIT)
	{
		m_nCycleProgress = 0;
	}
}
