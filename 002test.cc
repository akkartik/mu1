//: A simple test harness. To create new tests, define functions starting with
//: 'test_'. To run all tests so defined, run:
//:   $ ./mu test
//:
//: Every layer should include tests, and can reach into previous layers.
//: However, it seems like a good idea never to reach into tests from previous
//: layers. Every test should be a contract that always passes as originally
//: written, regardless of any later layers. Avoid writing 'temporary' tests
//: that are only meant to work until some layer.

:(before "End Types")
typedef void (*test_fn)(void);
:(before "Globals")
// move a global ahead into types that we can't generate an extern declaration for
const test_fn Tests[] = {
  #include "test_list"  // auto-generated; see 'build*' scripts
};

:(before "End Globals")
bool Run_tests = false;
bool Passed = true;  // set this to false inside any test to indicate failure

:(before "End Includes")
#define CHECK(X) \
  if (Passed && !(X)) { \
    cerr << "\nF - " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << "): " << #X << '\n'; \
    Passed = false; \
    return;  /* Currently we stop at the very first failure. */ \
  }

#define CHECK_EQ(X, Y) \
  if (Passed && (X) != (Y)) { \
    cerr << "\nF - " << __FUNCTION__ << "(" << __FILE__ << ":" << __LINE__ << "): " << #X << " == " << #Y << '\n'; \
    cerr << "  got " << (X) << '\n';  /* BEWARE: multiple eval */ \
    Passed = false; \
    return;  /* Currently we stop at the very first failure. */ \
  }

:(before "End Reset")
Passed = true;

:(before "End Commandline Parsing")
if (argc > 1 && is_equal(argv[1], "test")) {
  Run_tests = true;  --argc;  ++argv;  // shift 'test' out of commandline args
}

:(before "End Main")
if (Run_tests) {
  // Test Runs
  // we run some tests and then exit; assume no state need be maintained afterward

  long num_failures = 0;
  // End Test Run Initialization
  time_t t;  time(&t);
  cerr << "C tests: " << ctime(&t);
  for (size_t i=0;  i < sizeof(Tests)/sizeof(Tests[0]);  ++i) {
//?     cerr << "running " << Test_names[i] << '\n';
    run_test(i);
    if (Passed) cerr << '.';
    else ++num_failures;
  }
  cerr << '\n';
  // End Tests
  if (num_failures > 0) {
    cerr << num_failures << " failure"
         << (num_failures > 1 ? "s" : "")
         << '\n';
    return 1;
  }
  return 0;
}

:(code)
void run_test(size_t i) {
  if (i >= sizeof(Tests)/sizeof(Tests[0])) {
    cerr << "no test " << i << '\n';
    return;
  }
  reset();
  // End Test Setup
  (*Tests[i])();
  // End Test Teardown
}

//: Convenience: run a single test
:(before "Globals")
// Names for each element of the 'Tests' global, respectively.
const string Test_names[] = {
  #include "test_name_list"  // auto-generated; see 'build*' scripts
};
:(after "Test Runs")
string maybe_single_test_to_run = argv[argc-1];
if (!starts_with(maybe_single_test_to_run, "test_"))
  maybe_single_test_to_run.insert(0, "test_");
for (size_t i=0;  i < sizeof(Tests)/sizeof(Tests[0]);  ++i) {
  if (Test_names[i] == maybe_single_test_to_run) {
    run_test(i);
    if (Passed) cerr << ".\n";
    return 0;
  }
}

:(before "End Includes")
#include <stdlib.h>
