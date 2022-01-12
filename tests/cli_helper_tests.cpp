#include <gtest/gtest.h>

#include "cli_args_helper.h"

TEST(CliHelperTests, AllArgumentsTest)
{
	const int iArgumentsCount = 5;
	const char *cszArgs[] = {
		"cli_prog",
		"www.google.com",
		"--force",
		"--file",
		"some_random_file.html"
	};
	
	const CLI::CCLIArgsHelper cParameters{iArgumentsCount, cszArgs};

	ASSERT_STREQ(cParameters.GetName().c_str(), cszArgs[0]);

	// --force parameter should have empty value
	ASSERT_TRUE(cParameters.CheckIfParameterExist("force"));
	ASSERT_STREQ(cParameters.GetParameterValue("force")->c_str(), "");

	ASSERT_TRUE(cParameters.CheckIfParameterExist("file"));
	ASSERT_STREQ(cParameters.GetParameterValue("file")->c_str(), cszArgs[4]);

	ASSERT_STREQ(cParameters.GetParameterValue("")->c_str(), cszArgs[1]);
}

TEST(CliHelperTests, PartialArgumentsTest)
{
	const int iArgumentsCount = 3;
	const char *cszArgs[] = {
		"cli_prog",
		"www.google.com",
		"--force",
	};

	const CLI::CCLIArgsHelper cParameters{iArgumentsCount, cszArgs};

	ASSERT_STREQ(cParameters.GetName().c_str(), cszArgs[0]);

	ASSERT_TRUE(cParameters.CheckIfParameterExist("force"));
	ASSERT_STREQ(cParameters.GetParameterValue("force")->c_str(), "");

	ASSERT_STREQ(cParameters.GetParameterValue("")->c_str(), cszArgs[1]);

	ASSERT_FALSE(cParameters.CheckIfParameterExist("file"));
	ASSERT_EQ(cParameters.GetParameterValue("file"), std::nullopt);
}

TEST(CliHelperTests, FailTest)
{
	const int iArgumentsCount = 0;
	const char *cszArgs[] = {nullptr};

	const CLI::CCLIArgsHelper cParameters{iArgumentsCount, cszArgs};

	ASSERT_FALSE(cParameters.CheckIfParameterExist("force"));
	ASSERT_EQ(cParameters.GetParameterValue("force"), std::nullopt);

	ASSERT_FALSE(cParameters.CheckIfParameterExist("file"));
	ASSERT_EQ(cParameters.GetParameterValue("file"), std::nullopt);
}
