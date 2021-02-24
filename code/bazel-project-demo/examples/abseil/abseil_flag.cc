#include "absl/flags/flag.h"
#include "absl/time/time.h"

// 在.cc文件中定义flag
ABSL_FLAG(bool, big_menu, true,
          "Include 'advanced' options in the menu listing");
ABSL_FLAG(std::string, output_dir, "foo/bar/baz/", "output file dir");
ABSL_FLAG(std::vector<std::string>, languages,
          std::vector<std::string>({"english", "french", "german"}),
          "comma-separated list of languages to offer in the 'lang' menu");
ABSL_FLAG(absl::Duration, timeout, absl::Seconds(30), "Default RPC deadline");

// 在头文件声明flag ABSL_DECLARE_FLAG(absl::Duration, timeout);


int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  return 0;
}
