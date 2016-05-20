/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "libhsakmt.h"
#include "pmc_table.h"


static uint32_t kaveri_sq_counter_ids[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,
    83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
    134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 168, 169, 170,
    171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
    187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202,
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218,
    219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234,
    235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250
};

/* Unused counters - 166, 292 - 297 */
static uint32_t carrizo_sq_counter_ids[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,
	83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165,
	167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182,
	183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
	199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
	215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
	247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262,
	263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278,
	279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 298
};

/* Unused counters - 166, 292 - 297 */
static uint32_t fiji_sq_counter_ids[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82,
	83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
	102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
	150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165,
	167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182,
	183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198,
	199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
	215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
	247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 262,
	263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278,
	279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 298
};

/* Unused counters - 163 - 166, 167 and 251 are *_DUMMY_LAST */
static uint32_t hawaii_sq_counter_ids[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
    113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142,
    143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157,
    158, 159, 160, 161, 162, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
    178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
    193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222,
    223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237,
    238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250
};

static struct perf_counter_block kaveri_blocks[PERFCOUNTER_BLOCKID__MAX] = {
    [PERFCOUNTER_BLOCKID__SQ] = {
        .num_of_slots = 8,
        .num_of_counters = sizeof(kaveri_sq_counter_ids)/sizeof(*kaveri_sq_counter_ids),
        .counter_ids = kaveri_sq_counter_ids,
        .counter_size_in_bits = 64,
        .counter_mask = BITMASK(64)
    },
};

static struct perf_counter_block carrizo_blocks[PERFCOUNTER_BLOCKID__MAX] = {
    [PERFCOUNTER_BLOCKID__SQ] = {
        .num_of_slots = 8,
        .num_of_counters = sizeof(carrizo_sq_counter_ids)/sizeof(*carrizo_sq_counter_ids),
        .counter_ids = carrizo_sq_counter_ids,
        .counter_size_in_bits = 64,
        .counter_mask = BITMASK(64)
    },
};

static struct perf_counter_block fiji_blocks[PERFCOUNTER_BLOCKID__MAX] = {
	[PERFCOUNTER_BLOCKID__SQ] = {
		.num_of_slots = 8,
		.num_of_counters = sizeof(fiji_sq_counter_ids) / sizeof(*fiji_sq_counter_ids),
		.counter_ids = fiji_sq_counter_ids,
		.counter_size_in_bits = 64,
		.counter_mask = BITMASK(64)
	},
};

static struct perf_counter_block hawaii_blocks[PERFCOUNTER_BLOCKID__MAX] = {
    [PERFCOUNTER_BLOCKID__SQ] = {
        .num_of_slots = 8,
        .num_of_counters =
            sizeof(hawaii_sq_counter_ids) / sizeof(*hawaii_sq_counter_ids),
        .counter_ids = hawaii_sq_counter_ids,
        .counter_size_in_bits = 64,
        .counter_mask = BITMASK(64)
    },
};

HSAKMT_STATUS
get_block_properties(uint16_t dev_id,
                     enum perf_block_id block_id,
                     struct perf_counter_block *block)
{
    HSAKMT_STATUS rc = HSAKMT_STATUS_SUCCESS;
    if (block_id > PERFCOUNTER_BLOCKID__MAX || block_id < PERFCOUNTER_BLOCKID__FIRST)
        return HSAKMT_STATUS_INVALID_PARAMETER;

    switch(dev_id) {
        case 0x1304:
        case 0x1305:
        case 0x1306:
        case 0x1307:
        case 0x1309:
        case 0x130A:
        case 0x130B:
        case 0x130C:
        case 0x130D:
        case 0x130E:
        case 0x130F:
        case 0x1310:
        case 0x1311:
        case 0x1312:
        case 0x1313:
        case 0x1315:
        case 0x1316:
        case 0x1317:
        case 0x1318:
        case 0x131B:
        case 0x131C:
        case 0x131D:
            *block = kaveri_blocks[block_id];
            break;

        case 0x9870:
        case 0x9874:
        case 0x9875:
        case 0x9876:
        case 0x9877:
            *block = carrizo_blocks[block_id];
            break;

        case 0x7300:
            *block = fiji_blocks[block_id];
            break;

        case 0x67A0:
        case 0x67A1:
        case 0x67A2:
        case 0x67A8:
        case 0x67A9:
        case 0x67AA:
        case 0x67B0:
        case 0x67B1:
        case 0x67B8:
        case 0x67B9:
        case 0x67BA:
        case 0x67BE:
            *block = hawaii_blocks[block_id];
            break;

        default:
            rc = HSAKMT_STATUS_INVALID_PARAMETER;
    }

    return rc;
}


