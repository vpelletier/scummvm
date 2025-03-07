/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GOB_DETECTION_H
#define GOB_DETECTION_H

#include "engines/advancedDetector.h"

namespace Gob {

// WARNING: Reordering these will invalidate save games!
//          Add new games to the bottom of the list.
enum GameType {
	kGameTypeNone = 0,
	kGameTypeGob1,
	kGameTypeGob2,
	kGameTypeGob3,
	kGameTypeWoodruff,
	kGameTypeBargon,
	kGameTypeWeen,
	kGameTypeLostInTime,
	kGameTypeInca2,
	kGameTypeDynasty,
	kGameTypeUrban,
	kGameTypePlaytoons,
	kGameTypeBambou,
	kGameTypeFascination,
	kGameTypeGeisha,
	kGameTypeAdi2,
	kGameTypeAdi4,
	kGameTypeAdibou2,
	kGameTypeAdibou1,
	kGameTypeAbracadabra,
	kGameTypeBabaYaga,
	kGameTypeLittleRed,
	kGameTypeOnceUponATime, // Need more inspection to see if Baba Yaga or Abracadabra
	//kGameTypeAJWorld -> Deprecated, duplicated with kGameTypeAdibou1
	kGameTypeCrousti = 24, // Explicit value needed to not invalidate save games after removing kGameTypeAJWorld
	kGameTypeDynastyWood
};

enum Features {
	kFeaturesNone      =      0,
	kFeaturesCD        = 1 << 0,
	kFeaturesEGA       = 1 << 1,
	kFeaturesAdLib     = 1 << 2,
	kFeaturesSCNDemo   = 1 << 3,
	kFeaturesBATDemo   = 1 << 4,
	kFeatures640x480   = 1 << 5,
	kFeatures800x600   = 1 << 6,
	kFeaturesTrueColor = 1 << 7,
	kFeatures16Colors  = 1 << 8
};

enum AdditionalGameFlags {
	GF_ENABLE_ADIBOU2_FREE_BANANAS_WORKAROUND = 1 << 0,
	GF_ENABLE_ADIBOU2_FLOWERS_INFINITE_LOOP_WORKAROUND = 1 << 1,
};

struct GOBGameDescription {
	ADGameDescription desc;

	GameType gameType;
	int32 features;
	const char *startStkBase;
	const char *startTotBase;
	uint32 demoIndex;
};

} // End of namespace Gob

#endif // GOB_DETECTION_H
