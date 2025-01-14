#include <iostream>
#include <cstdlib>
#include <string.h>
#include <getopt.h>
#include <cassert>
#include <vector>
#include <string>

#ifdef _MSC_VER
#define strdup _strdup
#endif

char **vec2array(const std::vector<std::string> &vec) {
    char **arr = new char*[vec.size()];
    for (unsigned int ii = 0; ii < (unsigned int)vec.size(); ++ii) {
        arr[ii] = strdup(vec[ii].c_str());
    }
    return arr;
}

static void release(char **arr, size_t size) {
    for (size_t ii = 0; ii < size; ++ii) {
        free(arr[ii]);
    }
    delete []arr;
}

typedef std::vector<std::string> getoptvec;

static void getopt_test_0(void) {
    getoptvec vec;
    vec.push_back("program");
    vec.push_back("-a");
    vec.push_back("-b");
    int argc = (int)vec.size();
    char **argv = vec2array(vec);

    assert('a' == getopt(argc, argv, "a"));
    assert('?' == getopt(argc, argv, "a"));
    assert(optind = 2);

    release(argv, vec.size());
}

static void getopt_test_1(void) {
    getoptvec vec;
    vec.push_back("program");
    vec.push_back("-a");
    vec.push_back("--");
    vec.push_back("-b");
    int argc = (int)vec.size();
    char **argv = vec2array(vec);
    assert('a' == getopt(argc, argv, "a"));
    assert(-1 == getopt(argc, argv, "a"));
    assert(optind = 3);

    release(argv, vec.size());
}

static void getopt_test_2(void) {
    getoptvec vec;

    vec.push_back("..\\memcached\\engine_testapp");
    vec.push_back("-E");
    vec.push_back("ep.dll");
    vec.push_back("-T");
    vec.push_back("ep_testsuite.dll");
    vec.push_back("-e");
    vec.push_back("flushall_enabled=true;ht_size=13;ht_locks=7");
    vec.push_back("-v");
    vec.push_back("-C");
    vec.push_back("7");
    vec.push_back("-s");
    vec.push_back("foo");

    int argc = (int)vec.size();
    char **argv = vec2array(vec);

    assert('E' == getopt(argc, argv, "E:T:e:vC:s"));
    assert(strcmp(argv[2], optarg) == 0);
    assert('T' == getopt(argc, argv, "E:T:e:vC:s"));
    assert(strcmp(argv[4], optarg) == 0);
    assert('e' == getopt(argc, argv, "E:T:e:vC:s"));
    assert(strcmp(argv[6], optarg) == 0);
    assert('v' == getopt(argc, argv, "E:T:e:vC:s"));
    assert('C' == getopt(argc, argv, "E:T:e:vC:s"));
    assert(strcmp(argv[9], optarg) == 0);
    assert('s' == getopt(argc, argv, "E:T:e:vC:s"));
    assert(-1 == getopt(argc, argv, "E:T:e:vC:s"));
    assert(optind == 11);

    release(argv, vec.size());
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [testcase]" << std::endl;
        return 1;
    }

    switch (atoi(argv[1])) {
    case 0:
        getopt_test_0();
        break;
    case 1:
        getopt_test_1();
        break;
    case 2:
        getopt_test_2();
        break;
    default:
        std::cerr << "Unknown test case" << std::endl;
        return 1;
    }

    return 0;
}
