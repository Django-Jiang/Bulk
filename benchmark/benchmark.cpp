#include <cstdio>
#include <vector>

#include <bulk/bulk.hpp>

#include "../examples/set_backend.hpp"
#include "fit.hpp"
#include "report.hpp"
#include "timer.hpp"

double flop_rate(bulk::world& world) {
    unsigned int r_size = 1 << 23;
    std::vector<float> xs(r_size);
    std::iota(xs.begin(), xs.end(), 0);
    std::vector<float> ys = xs;
    std::vector<float> zs = xs;

    auto clock = bulk::util::timer();

    auto alpha = 1.0f / 3.0f;
    auto beta = 4.0f / 9.0f;
    for (auto i = 0u; i < r_size; ++i) {
        zs[i] = zs[i] + alpha * xs[i] - beta * ys[i];
    }

    auto total_ms = clock.get();

    auto flops = r_size * 4;

    auto flops_per_s = bulk::gather_all(world, 1000.0 * flops / total_ms);
    return bulk::util::average(flops_per_s);
}

int main() {
    environment env;

    env.spawn(env.available_processors(), [](bulk::world& world) {
        int s = world.processor_id();
        int p = world.active_processors();

        // about 4 MB
        unsigned int size = 10'000'000u;

        std::vector<int> dummy_data(size);
        std::iota(dummy_data.begin(), dummy_data.end(), 0);

        auto r = flop_rate(world);

        bulk::coarray<int> target(world, size);

        std::vector<size_t> test_sizes = {
            4u,       8u,         64u,        512u,       1024u,
            2'000u,   4'000u,     8'000u,     16'000u,    32'000u,
            64'000u,  100'000u,   128'000u,   200'000u,   256'000u,
            512'000u, 1'000'000u, 4'000'000u, 8'000'000u, 10'000'000u};
        std::vector<double> test_results;

        for (auto test_size : test_sizes) {
            double total = 0.0f;
            int sample_size = p > 8 ? 8 : p - 1;
            for (auto k = 0; k < sample_size; ++k) {
                auto t = (world.next_processor() + k * (p / sample_size)) % p;

                auto clock = bulk::util::timer();

                target.put(t, dummy_data.begin(),
                           dummy_data.begin() + test_size);
                world.sync();

                auto total_ms = clock.get();
                total += total_ms;
            }

            test_results.push_back(total / sample_size);
        }

        auto parameters = bulk::util::fit(test_sizes, test_results);
        if (parameters) {
            auto l = parameters.value().first * (r / 1000.0);
            auto g = parameters.value().second * (r / 1000.0);
            auto rs = bulk::gather_all(world, r / 1e9);
            auto gs = bulk::gather_all(world, g);
            auto ls = bulk::gather_all(world, l);

            if (s == 0) {
                auto comm_report =
                    bulk::util::table("Communication times", "size");
                comm_report.columns("time");

                for (auto i = 0u; i < test_results.size(); ++i) {
                    comm_report.row(std::to_string(test_sizes[i]),
                                    test_results[i]);
                }

                world.log(comm_report.print().c_str());
                world.log("fit: [time] = %f + %f [size]", parameters.value().first,
                          parameters.value().second);
                world.log("--\n");

                std::vector<double> durations;
                for (int i = 0; i < 100; ++i) {
                    auto a = bulk::util::timer();
                    auto dt = a.get();
                    durations.push_back(dt * (r / 1000.0));
                }

                auto avg_r = bulk::util::average(rs);

                auto report = bulk::util::table("BSP parameters", "parameter");
                report.columns("value", "unit");

                report.row("p", p);
                report.row("r", avg_r, "GFLOPS");
                report.row("g", bulk::util::average(gs), "FLOPs/word");
                report.row("l", bulk::util::average(ls), "FLOPs");
                report.row("total", avg_r * p, "GFLOPS");
                report.row("clock", bulk::util::average(durations), "FLOPs");

                world.log(report.print().c_str());
            }
        }
    });

    return 0;
}
