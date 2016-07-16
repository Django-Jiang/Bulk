#include <bulk/environment.hpp>
#include <bulk/epiphany/host.hpp>
#define CATCH_CONFIG_MAIN
#include <catch.hpp>

TEST_CASE("basic communication", "[communication]") {
    bulk::environment<bulk::epiphany::provider> env;

    REQUIRE( env.provider().is_valid() );

    int success_count;

    env.provider().setLogCallback([&success_count](int pid, const std::string& log) {
        if (log.substr(0, 7) == "SUCCESS")
            success_count++;
        else
            std::cout << "Core " << pid << ": " << log << std::endl;
    });

    SECTION("var put") {
        success_count = 0;
        env.spawn(env.available_processors(), "test_kernel1.elf");
        CHECK( success_count == env.available_processors() );
    }

    SECTION("multiple var put") {
        success_count = 0;
        env.spawn(env.available_processors(), "test_kernel2.elf");
        CHECK( success_count == env.available_processors() * 5 );
    }

    SECTION("coarrays") {
        success_count = 0;
        env.spawn(env.available_processors(), "test_kernel3.elf");
        CHECK( success_count == env.available_processors() * 2 );
    }
}
