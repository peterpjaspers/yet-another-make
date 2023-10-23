#include "../Glob.h"
#include "gtest/gtest.h"

namespace {
    using namespace YAM;

    void assertMatch(std::string const& globPattern, std::string const& str, bool globstar = true) {
        Glob glob(globPattern, globstar);
        bool match = glob.matches(str);
        if (match) {
            EXPECT_TRUE(glob.matches(str));
        } else {
            EXPECT_TRUE(glob.matches(str));
        }
    }
    void assertNotMatch(std::string const& globPattern, std::string const& str, bool globstar = true) {
        Glob glob(globPattern, globstar);
        bool match = glob.matches(str);
        if (match) {
            EXPECT_FALSE(glob.matches(str));
        } else {
            EXPECT_FALSE(glob.matches(str));
        }
    }

    void test(bool globstar) {
        // Match everything
        assertMatch("*", "foo", globstar);

        // Match the end
        assertMatch("f*", "foo", globstar);

        // Match the start
        assertMatch("*o", "foo", globstar);

        // Match the middle
        assertMatch("f*uck", "firetruck", globstar);

        // Match anywhere
        assertMatch("*uc*", "firetruck", globstar);

        // Do not match anywhere
        assertNotMatch("uc", "firetruck", globstar);

        // Match zero characters;
        assertMatch("f*uck", "fuck", globstar);

        // More complex matches
        assertMatch("*.min.js", "http://example.com/jquery.min.js", false);
        assertMatch("*.min.*", "http://example.com/jquery.min.js", false);
        assertMatch("*/js/*.js", "http://example.com/js/jquery.min.js", false);
        assertNotMatch("*.min.js", "http://example.com/jquery.min.js", true);
        assertNotMatch("*.min.*", "http://example.com/jquery.min.js", true);
        assertNotMatch("*/js/*.js", "http://example.com/js/jquery.min.js", true);

        // Test string  "\\\\/$^+?.()=!|{},[].*"  represents  <glob>\\/$^+?.()=!|{},[].*</glob>
        // The equivalent regex is:  /^\\\/\$\^\+?\.\(\)\=\!\|{}\,[]\..*$/
        // Both glob and regex match:  \/$^+?.()=!|{},[].*
        std::string globStr =   R"(\\/$^+?.()=!|{},[].*)";
        std::string targetStr = R"(\/$^+?.()=!|{},[].*)";
        //assertMatch(globStr, targetStr, globstar); 

        assertNotMatch(".min.", "http://example.com/jquery.min.js", globstar);
        assertNotMatch("*.min.*", "http://example.com/jquery.min.js", true);
        assertMatch("*.min.*", "http://example.com/jquery.min.js", false);

        assertNotMatch("http:", "http://example.com/jquery.min.js", globstar);
        assertNotMatch("http:*", "http://example.com/jquery.min.js", true);
        assertMatch("http:*", "http://example.com/jquery.min.js", false);

        assertNotMatch("min.js", "http://example.com/jquery.min.js", globstar);
        assertNotMatch("*.min.js", "http://example.com/jquery.min.js", true);
        assertMatch("*.min.js", "http://example.com/jquery.min.js", false);

        // Do not match anywhere 
        assertNotMatch("min", "http://example.com/jquery.min.js", globstar);
        assertNotMatch("/js/", "http://example.com/js/jquery.min.js", globstar);
        assertNotMatch("/js*jq*.js", "http://example.com/js/jquery.min.js", globstar);

        // ?: Match one character, no more and no less
        assertMatch("f?o", "foo", globstar);
        assertNotMatch("f?o", "fooo", globstar);
        assertNotMatch("f?oo", "foo", globstar);

        // []: Match a character range
        assertMatch("fo[oz]", "foo", globstar);
        assertMatch("fo[oz]", "foz", globstar);
        assertNotMatch("fo[oz]", "fog", globstar);

        // {}: Match a choice of different substrings
        assertMatch("foo{bar,baaz}", "foobaaz", globstar);
        assertMatch("foo{bar,baaz}", "foobar", globstar);
        assertNotMatch("foo{bar,baaz}", "foobuzz", globstar);
        assertMatch("foo{bar,b*z}", "foobuzz", globstar);

        // More complex matches
        assertMatch("http://?o[oz].b*z.com/{*.js,*.html}",
                    "http://foo.baaz.com/jquery.min.js", globstar);
        assertMatch("http://?o[oz].b*z.com/{*.js,*.html}",
                    "http://moz.buzz.com/index.html", globstar);
        assertNotMatch("http://?o[oz].b*z.com/{*.js,*.html}",
                       "http://moz.buzz.com/index.htm", globstar);
        assertNotMatch("http://?o[oz].b*z.com/{*.js,*.html}",
                       "http://moz.bar.com/index.html", globstar);
        assertNotMatch("http://?o[oz].b*z.com/{*.js,*.html}",
                       "http://flozz.buzz.com/index.html", globstar);


        assertMatch("http://foo.com/**/{*.js,*.html}",
                    "http://foo.com/bar/jquery.min.js",
                    globstar);
        assertMatch("http://foo.com/**/{*.js,*.html}",
                    "http://foo.com/bar/baz/jquery.min.js",
                    globstar);
        assertMatch("http://foo.com/**",
                    "http://foo.com/bar/baz/jquery.min.js",
                    globstar);

        // Remaining special chars should still match themselves
        // Test string  "\\\\/$^+.()=!|,.*"  represents  <glob>\\/$^+.()=!|,.*</glob>
        // The equivalent regex is:  /^\\\/\$\^\+\.\(\)\=\!\|\,\..*$/
        // Both glob and regex match:  \/$^+.()=!|,.*
        std::string testExtStr = R"(\\\\/$^+.()=!|,.*)";
        std::string targetExtStr = R"(\\/$^+.()=!|,.*)";
        assertMatch(testExtStr, targetExtStr);
        assertMatch(testExtStr, targetExtStr, globstar);

        // globstar specific tests
        assertMatch("/foo/*", "/foo/bar.txt", true);
        assertMatch("/foo/**", "/foo/baz.txt", true);
        assertMatch("/foo/**", "/foo/bar/baz.txt", true);
        assertMatch("/foo/*/*.txt", "/foo/bar/baz.txt", true);
        assertMatch("/foo/**/*.txt", "/foo/bar/baz.txt", true);
        assertMatch("/foo/**/*.txt", "/foo/bar/baz/qux.txt", true);
        assertMatch("/foo/**/bar.txt", "/foo/bar.txt", true);
        assertMatch("/foo/**/**/bar.txt", "/foo/bar.txt", true);
        assertMatch("/foo/**/*/baz.txt", "/foo/bar/baz.txt", true);
        assertMatch("/foo/**/*.txt", "/foo/bar.txt", true);
        assertMatch("/foo/**/**/*.txt", "/foo/bar.txt", true);
        assertMatch("/foo/**/*/*.txt", "/foo/bar/baz.txt", true);
        assertMatch("**/*.txt", "/foo/bar/baz/qux.txt", true);
        assertMatch("**/foo.txt", "foo.txt", true);
        assertMatch("**/*.txt", "foo.txt", true);

        assertNotMatch("/foo/*", "/foo/bar/baz.txt", true);
        assertNotMatch("/foo/*.txt", "/foo/bar/baz.txt", true);
        assertNotMatch("/foo/*/*.txt", "/foo/bar/baz/qux.txt", true);
        assertNotMatch("/foo/*/bar.txt", "/foo/bar.txt", true);
        assertNotMatch("/foo/*/*/baz.txt", "/foo/bar/baz.txt", true);
        assertNotMatch("/foo/**.txt", "/foo/bar/baz/qux.txt", true);
        assertNotMatch("/foo/bar**/*.txt", "/foo/bar/baz/qux.txt", true);
        assertNotMatch("/foo/bar**", "/foo/bar/baz.txt", true);
        assertNotMatch("**/.txt", "/foo/bar/baz/qux.txt", true);
        assertNotMatch("*/*.txt", "/foo/bar/baz/qux.txt", true);
        assertNotMatch("*/*.txt", "foo.txt", true);

        assertNotMatch("http://foo.com/*",
                       "http://foo.com/bar/baz/jquery.min.js",
                       true);
        assertNotMatch("http://foo.com/*",
                       "http://foo.com/bar/baz/jquery.min.js",
                       true);

        assertMatch("http://foo.com/*",
                    "http://foo.com/bar/baz/jquery.min.js",
                    false);
        assertMatch("http://foo.com/**",
                    "http://foo.com/bar/baz/jquery.min.js",
                    true);

        assertMatch("http://foo.com/*/*/jquery.min.js",
                    "http://foo.com/bar/baz/jquery.min.js",
                    true);
        assertMatch("http://foo.com/**/jquery.min.js",
                    "http://foo.com/bar/baz/jquery.min.js",
                    true);
        assertMatch("http://foo.com/*/*/jquery.min.js",
                    "http://foo.com/bar/baz/jquery.min.js",
                    false);
        assertMatch("http://foo.com/*/jquery.min.js",
                    "http://foo.com/bar/baz/jquery.min.js",
                    false);
        assertNotMatch("http://foo.com/*/jquery.min.js",
                       "http://foo.com/bar/baz/jquery.min.js",
                       true);

    }

    TEST(Glob, globstar) {
        test(true);
    }

    TEST(Glob, noGlobstar) {
        test(false);
    }

}