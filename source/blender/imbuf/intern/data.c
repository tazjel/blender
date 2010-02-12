/**
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * data.c
 *
 * $Id$
 */

#include "imbuf.h"
#include "matrix.h"

/*
static short quadbase[31] = {
	150,133,117,102,
	88,75,63,52,
	42,33,25,18,
	12,7,3,0,
	3,7,12,18,
	25,33,42,52,
	63,75,88,102,
	117,133,150,
};

short *quadr = quadbase+15;
*/
/*
main()
{
	ushort _quadr[511], *quadr;
	int i, delta;
	
	quadr = _quadr + 255;

	delta = 0;
	for (i = 0 ; i <= 255 ; i++){
		quadr[i] = quadr[-i] = delta;
		delta += i + 3;
	}
	
	delta = 0;
	for (i = 0; i < 511; i++){
		printf("%6d, ", _quadr[i]);
		delta++;
		if (delta == 8){
			delta = 0;
			printf("\n");
		}
	}
}
*/

static unsigned short quadbase[511] = {
 33150,  32893,  32637,  32382,  32128,  31875,  31623,  31372, 
 31122,  30873,  30625,  30378,  30132,  29887,  29643,  29400, 
 29158,  28917,  28677,  28438,  28200,  27963,  27727,  27492, 
 27258,  27025,  26793,  26562,  26332,  26103,  25875,  25648, 
 25422,  25197,  24973,  24750,  24528,  24307,  24087,  23868, 
 23650,  23433,  23217,  23002,  22788,  22575,  22363,  22152, 
 21942,  21733,  21525,  21318,  21112,  20907,  20703,  20500, 
 20298,  20097,  19897,  19698,  19500,  19303,  19107,  18912, 
 18718,  18525,  18333,  18142,  17952,  17763,  17575,  17388, 
 17202,  17017,  16833,  16650,  16468,  16287,  16107,  15928, 
 15750,  15573,  15397,  15222,  15048,  14875,  14703,  14532, 
 14362,  14193,  14025,  13858,  13692,  13527,  13363,  13200, 
 13038,  12877,  12717,  12558,  12400,  12243,  12087,  11932, 
 11778,  11625,  11473,  11322,  11172,  11023,  10875,  10728, 
 10582,  10437,  10293,  10150,  10008,   9867,   9727,   9588, 
  9450,   9313,   9177,   9042,   8908,   8775,   8643,   8512, 
  8382,   8253,   8125,   7998,   7872,   7747,   7623,   7500, 
  7378,   7257,   7137,   7018,   6900,   6783,   6667,   6552, 
  6438,   6325,   6213,   6102,   5992,   5883,   5775,   5668, 
  5562,   5457,   5353,   5250,   5148,   5047,   4947,   4848, 
  4750,   4653,   4557,   4462,   4368,   4275,   4183,   4092, 
  4002,   3913,   3825,   3738,   3652,   3567,   3483,   3400, 
  3318,   3237,   3157,   3078,   3000,   2923,   2847,   2772, 
  2698,   2625,   2553,   2482,   2412,   2343,   2275,   2208, 
  2142,   2077,   2013,   1950,   1888,   1827,   1767,   1708, 
  1650,   1593,   1537,   1482,   1428,   1375,   1323,   1272, 
  1222,   1173,   1125,   1078,   1032,    987,    943,    900, 
   858,    817,    777,    738,    700,    663,    627,    592, 
   558,    525,    493,    462,    432,    403,    375,    348, 
   322,    297,    273,    250,    228,    207,    187,    168, 
   150,    133,    117,    102,     88,     75,     63,     52, 
    42,     33,     25,     18,     12,      7,      3,      0, 
     3,      7,     12,     18,     25,     33,     42,     52, 
    63,     75,     88,    102,    117,    133,    150,    168, 
   187,    207,    228,    250,    273,    297,    322,    348, 
   375,    403,    432,    462,    493,    525,    558,    592, 
   627,    663,    700,    738,    777,    817,    858,    900, 
   943,    987,   1032,   1078,   1125,   1173,   1222,   1272, 
  1323,   1375,   1428,   1482,   1537,   1593,   1650,   1708, 
  1767,   1827,   1888,   1950,   2013,   2077,   2142,   2208, 
  2275,   2343,   2412,   2482,   2553,   2625,   2698,   2772, 
  2847,   2923,   3000,   3078,   3157,   3237,   3318,   3400, 
  3483,   3567,   3652,   3738,   3825,   3913,   4002,   4092, 
  4183,   4275,   4368,   4462,   4557,   4653,   4750,   4848, 
  4947,   5047,   5148,   5250,   5353,   5457,   5562,   5668, 
  5775,   5883,   5992,   6102,   6213,   6325,   6438,   6552, 
  6667,   6783,   6900,   7018,   7137,   7257,   7378,   7500, 
  7623,   7747,   7872,   7998,   8125,   8253,   8382,   8512, 
  8643,   8775,   8908,   9042,   9177,   9313,   9450,   9588, 
  9727,   9867,  10008,  10150,  10293,  10437,  10582,  10728, 
 10875,  11023,  11172,  11322,  11473,  11625,  11778,  11932, 
 12087,  12243,  12400,  12558,  12717,  12877,  13038,  13200, 
 13363,  13527,  13692,  13858,  14025,  14193,  14362,  14532, 
 14703,  14875,  15048,  15222,  15397,  15573,  15750,  15928, 
 16107,  16287,  16468,  16650,  16833,  17017,  17202,  17388, 
 17575,  17763,  17952,  18142,  18333,  18525,  18718,  18912, 
 19107,  19303,  19500,  19698,  19897,  20097,  20298,  20500, 
 20703,  20907,  21112,  21318,  21525,  21733,  21942,  22152, 
 22363,  22575,  22788,  23002,  23217,  23433,  23650,  23868, 
 24087,  24307,  24528,  24750,  24973,  25197,  25422,  25648, 
 25875,  26103,  26332,  26562,  26793,  27025,  27258,  27492, 
 27727,  27963,  28200,  28438,  28677,  28917,  29158,  29400, 
 29643,  29887,  30132,  30378,  30625,  30873,  31122,  31372, 
 31623,  31875,  32128,  32382,  32637,  32893,  33150,
};

unsigned short *quadr = quadbase + 255;
