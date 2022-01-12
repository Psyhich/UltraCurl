#include "cli_progres.h"

char CLI::CCLIProgressPrinter::ReduceUnit(double &dBytesToReduce) noexcept
{
	int iUnitIndex = 0;
	while(dBytesToReduce / UNITS_BYTES[iUnitIndex] >= 1)
	{
		++iUnitIndex;
	}

	dBytesToReduce /= UNITS_BYTES[iUnitIndex];
	return UNITS[iUnitIndex];
}

void CLI::CCLIProgressPrinter::PrintBytesCell(double dBytesToPrint, char cUnitToPrint) noexcept
{
	// If we have bigger unit than simple bytes we can print with 2 numbers precision
	if(cUnitToPrint != 'B')
	{
		printf("%0.2lf", dBytesToPrint);
	}
	else
	{
		printf("  %0.0lf", dBytesToPrint);
	}

	printf("%c|", cUnitToPrint);

}

void CLI::CCLIProgressPrinter::PrintHelp() noexcept
{
	printf("|    URI    |  Down  | Overal |\n");
}

void CLI::CCLIProgressPrinter::PrintProgress(
	std::map<CURI, std::tuple<std::size_t, std::size_t>> dataToShow) noexcept
{
	for(auto &[uri, progress] : dataToShow)
	{
		std::string adressWithPath;
		if(auto pureAdress = uri.GetPureAddress())
		{
			adressWithPath = std::move(*pureAdress);
		}
		if(auto path = uri.GetPath())
		{
			adressWithPath += *path;
		}

		// Clipping address to fit to URI cell
		// if string is bigger than SIZE_OF_URI splitting to fit into URI
		if(adressWithPath.size() > SIZE_OF_URI_CELL)
		{
			adressWithPath = adressWithPath.substr(0, SIZE_OF_URI_CELL);
		}
		// If lesser adding spaces in the end
		else
		{
			for(std::size_t nCount = 0; nCount < SIZE_OF_URI_CELL; nCount++)
			{
				adressWithPath.push_back(' ');
			}
		}
		// Printing address
		printf("|%s|", adressWithPath.c_str());

		// Transforming bytes count into kilo, mega, giga, or petabytes representation
		double dBytesRecieved = std::get<0>(progress);
		const char cRecievedUnit = ReduceUnit(dBytesRecieved);
		// Printing recieved bytes
		PrintBytesCell(dBytesRecieved, cRecievedUnit);
		
		double dBytesDownload = std::get<1>(progress);
		const char cSizeUnit = ReduceUnit(dBytesDownload);
		PrintBytesCell(dBytesDownload, cSizeUnit);
		
	}

}
void CLI::CCLIProgressPrinter::Refresh() noexcept
{
	for(int i = 0; i < 100; i++)
	{
		printf("\n");
	}
}
