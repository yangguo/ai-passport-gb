#include "gb_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ROM_BYTES = 32768 };

static void fix_checksum(uint8_t *rom)
{
	uint8_t checksum = 0;

	for (size_t i = 0x134; i < 0x14D; i++)
		checksum = (uint8_t)(checksum - rom[i] - 1u);
	rom[0x14D] = checksum;
}

static int expect_result(const char *name, const uint8_t *rom, size_t size,
			gb_err_t expected)
{
	size_t save_size = 0;
	gb_core_t *core = calloc(1, gb_core_instance_size());
	gb_err_t save_result;
	gb_err_t init_result;

	if (core == NULL)
		return 1;
	save_result = gb_core_save_size(rom, size, &save_size);
	init_result = gb_core_init(core, rom, size, NULL, 0);
	free(core);
	if (save_result != expected || init_result != expected) {
		fprintf(stderr, "%s: save_size=%d init=%d expected=%d\n", name,
			(int)save_result, (int)init_result, (int)expected);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	uint8_t original[ROM_BYTES];
	uint8_t rom[ROM_BYTES];
	FILE *file;
	int failed = 0;
	const struct {
		const char *name;
		size_t offset;
		uint8_t value;
		gb_err_t expected;
	} headers[] = {
		{ "invalid ROM size code 9", 0x148, 9, GB_ERR_UNSUPPORTED },
		{ "invalid ROM size code 255", 0x148, 255, GB_ERR_UNSUPPORTED },
		{ "invalid RAM size code 6", 0x149, 6, GB_ERR_UNSUPPORTED },
		{ "invalid RAM size code 255", 0x149, 255, GB_ERR_UNSUPPORTED },
		{ "declared 64 KiB in 32 KiB file", 0x148, 1, GB_ERR_BAD_ARG },
		{ "unsupported cartridge", 0x147, 0xFE, GB_ERR_UNSUPPORTED },
	};

	if (argc != 2 || (file = fopen(argv[1], "rb")) == NULL)
		return 1;
	if (fread(original, 1, sizeof(original), file) != sizeof(original)) {
		fclose(file);
		return 1;
	}
	fclose(file);

	failed |= expect_result("valid ROM", original, sizeof(original), GB_OK);
	for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
		memcpy(rom, original, sizeof(rom));
		rom[headers[i].offset] = headers[i].value;
		fix_checksum(rom);
		failed |= expect_result(headers[i].name, rom, sizeof(rom),
					headers[i].expected);
	}
	failed |= expect_result("short header", original, 0x14F, GB_ERR_BAD_ARG);
	failed |= expect_result("header without ROM body", original, 0x150,
				GB_ERR_BAD_ARG);
	failed |= expect_result("ROM missing last byte", original, ROM_BYTES - 1,
				GB_ERR_BAD_ARG);
	memcpy(rom, original, sizeof(rom));
	rom[0x14D] ^= 1;
	failed |= expect_result("invalid checksum", rom, sizeof(rom),
				GB_ERR_CHECKSUM);
	if (!failed)
		puts("test_core: ok");
	return failed;
}
