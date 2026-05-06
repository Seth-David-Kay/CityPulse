#define PLATFORM_NONE 1
#include "base/base.c"
// Mine
#include <stdlib.h>
#include <time.h>

// returns a random float between min and max
f32 randf_range(f32 min, f32 max) {
    return min + (f32)rand() / (f32)RAND_MAX * (max - min);
}

// Box-Müller (normal distribution)
f32 rand_normal_dist(f32 mean, f32 stddev)
{
	// Because below we do a slight variation to not get 0 or 1 it's
	// 	not perfectly uniform, can change this by using normal box-muller
	// 	function an getting two rand variables but for now it's okay
	//	... we just don't want to log 0 b/c crash and log(1) is unexpected
	f32 u1 = ((f32)rand() + 1.0f) / ((f32)RAND_MAX + 2.0f);
	f32 u2 = ((f32)rand() + 1.0f) / ((f32)RAND_MAX + 2.0f);
	f32 x = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * pi_f32 * u2);
	f32 result = mean + x * stddev;
	// clamp
	result = clamp(result, 0, 1.0f);
	return result;
}

void generate_histogram(Arena *arena)
{
	u64 num_samples = 100000;
	u64 num_bins = 50;

	Array_i32 histogram = { .arena = arena };
	reserve(&histogram, num_bins);

	// Calling random number generator
	// CHANGE HERE* FOR TESTING NUMBER GENERATORS
	for (u64 i = 0; i < num_samples; i++)
	{
		f32 rand_num = rand_normal_dist(0.5f, 0.05f);
		u64 bin = u64)(rand_num * (f32)num_bins);
		bin = clamp(bin, 0, num_bins - 1);
		histogram.data[bin]++;
	}
	// Print histogram
	print("Histogram:\n");
	for (u64 i = 0; i < num_bins; i++)
	{
		f32 lowest_num = (f32)i / num_bins;
		f32 highest_num = (f32)(i + 1) / num_bins;
		print("[% - %): ", fmt(f32, lowest_num), fmt(f32, highest_num));
		int scale = histogram.data[i] / ((i32)num_samples / 200);
		if (histogram.data[i] > 0 && scale < 1)
			print(",");
		for (int j = 0; j < scale; j++)
			print("*");

		print("\n");
	}
}

// static void program(void)
// {
// 	// i64 r = (i64)rand_normal_dist(0.0, 3.0) % 10;
// 	// print("%\n", fmt(i64, r));

// 	Arena arena = arena_init(8 * 1024 * 1024);

// 	// Initialize the randomizer using the current timestamp as a seed
// 	srand((u32)time(NULL));
// 	// Testing code
// 	generate_histogram(&arena);
// }
