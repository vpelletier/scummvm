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

#define FORBIDDEN_SYMBOL_EXCEPTION_printf

#include "common/debug.h"
#include "common/util.h"
#include "common/file.h"
#include "common/macresman.h"
#include "common/md5.h"
#include "common/config-manager.h"
#include "common/punycode.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "common/translation.h"
#include "gui/EventRecorder.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "engines/advancedDetector.h"
#include "engines/obsolete.h"

/**
 * Adapter to be able to use Common::Archive based code from the AD.
 */
class FileMapArchive : public Common::Archive {
public:
	FileMapArchive(const AdvancedMetaEngineDetection::FileMap &fileMap) : _fileMap(fileMap) {}

	bool hasFile(const Common::Path &path) const override {
		Common::String name = path.toString();
		return _fileMap.contains(name);
	}

	int listMembers(Common::ArchiveMemberList &list) const override {
		int files = 0;
		for (AdvancedMetaEngineDetection::FileMap::const_iterator it = _fileMap.begin(); it != _fileMap.end(); ++it) {
			list.push_back(Common::ArchiveMemberPtr(new Common::FSNode(it->_value)));
			++files;
		}

		return files;
	}

	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override {
		Common::String name = path.toString();
		AdvancedMetaEngineDetection::FileMap::const_iterator it = _fileMap.find(name);
		if (it == _fileMap.end()) {
			return Common::ArchiveMemberPtr();
		}

		return Common::ArchiveMemberPtr(new Common::FSNode(it->_value));
	}

	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
		Common::String name = path.toString();
		Common::FSNode fsNode = _fileMap.getValOrDefault(name);
		return fsNode.createReadStream();
	}

private:
	const AdvancedMetaEngineDetection::FileMap &_fileMap;
};

static Common::String sanitizeName(const char *name, int maxLen) {
	Common::String res;
	Common::String word;
	Common::String lastWord;
	const char *origname = name;

	do {
		if (Common::isAlnum(*name)) {
			word += tolower(*name);
		} else {
			// Skipping short words and "the"
			if ((word.size() > 2 && !word.equals("the")) || (!word.empty() && Common::isDigit(word[0]))) {
				// Adding first word, or when word fits
				if (res.empty() || (int)word.size() < maxLen)
					res += word;

				maxLen -= word.size();
			}

			if ((*name && *(name + 1) == 0) || !*name) {
				if (res.empty()) // Make sure that we add at least something
					res += word.empty() ? lastWord : word;

				break;
			}

			if (!word.empty())
				lastWord = word;

			word.clear();
		}
		if (*name)
			name++;
	} while (maxLen > 0);

	if (res.empty())
		error("AdvancedDetector: Incorrect extra in game: \"%s\"", origname);

	return res;
}

/**
 * Generate a preferred target value as
 *   GAMEID-PLAFORM-LANG
 * or (if ADGF_DEMO has been set)
 *   GAMEID-demo-PLAFORM-LANG
 */
static Common::String generatePreferredTarget(const ADGameDescription *desc, int maxLen, Common::String targetID) {
	Common::String res;

	if (!targetID.empty()) {
		res = targetID;
	} else if (desc->flags & ADGF_AUTOGENTARGET && desc->extra && *desc->extra) {
		res = sanitizeName(desc->extra, maxLen);
	} else {
		res = desc->gameId;
	}

	if (desc->flags & ADGF_DEMO) {
		res = res + "-demo";
	}

	if (desc->flags & ADGF_CD) {
		res = res + "-cd";
	}

	if (desc->flags & ADGF_DVD) {
		res = res + "-dvd";
	}

	if (desc->flags & ADGF_REMASTERED) {
		res = res + "-remastered";
	}

	if (desc->platform != Common::kPlatformDOS && desc->platform != Common::kPlatformUnknown && !(desc->flags & ADGF_DROPPLATFORM)) {
		res = res + "-" + getPlatformAbbrev(desc->platform);
	}

	if (desc->language != Common::EN_ANY && desc->language != Common::UNK_LANG && !(desc->flags & ADGF_DROPLANGUAGE)) {
		res = res + "-" + getLanguageCode(desc->language);
	}

	return res;
}

DetectedGame AdvancedMetaEngineDetection::toDetectedGame(const ADDetectedGame &adGame, ADDetectedGameExtraInfo *extraInfo) const {
	const ADGameDescription *desc = adGame.desc;

	const char *title;
	const char *extra;
	if (desc->flags & ADGF_USEEXTRAASTITLE) {
		title = desc->extra;
		extra = "";
	} else {
		const PlainGameDescriptor *pgd = findPlainGameDescriptor(desc->gameId, _gameIds);
		if (pgd) {
			title = pgd->description;
		} else {
			title = "";
		}
		extra = desc->extra;
	}

	if (extraInfo) {
		if (!extraInfo->gameName.empty())
			title = extraInfo->gameName.c_str();
	}

	DetectedGame game(getName(), desc->gameId, title, desc->language, desc->platform, extra, ((desc->flags & (ADGF_UNSUPPORTED | ADGF_WARNING)) != 0));
	game.hasUnknownFiles = adGame.hasUnknownFiles;
	game.matchedFiles = adGame.matchedFiles;

	if (extraInfo && !extraInfo->targetID.empty()) {
		game.preferredTarget = generatePreferredTarget(desc, _maxAutogenLength, extraInfo->targetID);
	} else {
		game.preferredTarget = generatePreferredTarget(desc, _maxAutogenLength, Common::String());
	}

	game.gameSupportLevel = kStableGame;
	if (desc->flags & ADGF_UNSTABLE)
		game.gameSupportLevel = kUnstableGame;
	else if (desc->flags & ADGF_TESTING)
		game.gameSupportLevel = kTestingGame;
	else if (desc->flags & ADGF_UNSUPPORTED)
		game.gameSupportLevel = kUnsupportedGame;
	else if (desc->flags & ADGF_WARNING)
		game.gameSupportLevel = kWarningGame;

	game.setGUIOptions(desc->guiOptions + _guiOptions);
	game.appendGUIOptions(getGameGUIOptionsDescriptionLanguage(desc->language));

	if (desc->flags & ADGF_ADDENGLISH)
		game.appendGUIOptions(getGameGUIOptionsDescriptionLanguage(Common::EN_ANY));

	if (_flags & kADFlagUseExtraAsHint)
		game.extra = desc->extra;

	return game;
}

bool AdvancedMetaEngineDetection::cleanupPirated(ADDetectedGames &matched) const {
	// OKay, now let's sense presence of pirated games
	if (!matched.empty()) {
		for (uint j = 0; j < matched.size();) {
			if (matched[j].desc->flags & ADGF_PIRATED)
				matched.remove_at(j);
			else
				++j;
		}

		// We ruled out all variants and now have nothing
		if (matched.empty()) {
			warning("Illegitimate game copy detected. We provide no support in such cases");
			if (GUI::GuiManager::hasInstance()) {
				GUI::MessageDialog dialog(_("Illegitimate game copy detected. We provide no support in such cases"));
				dialog.runModal();
			};
			return true;
		}
	}

	return false;
}

DetectedGames AdvancedMetaEngineDetection::detectGames(const Common::FSList &fslist, uint32 skipADFlags, bool skipIncomplete) {
	FileMap allFiles;

	if (fslist.empty())
		return DetectedGames();

	// Sometimes this method is called directly, so we have to build the maps, especially
	// the _directoryGlobsMap
	preprocessDescriptions();

	// Compose a hashmap of all files in fslist.
	composeFileHashMap(allFiles, fslist, (_maxScanDepth == 0 ? 1 : _maxScanDepth));

	// Run the detector on this
	ADDetectedGames matches = detectGame(fslist.begin()->getParent(), allFiles, Common::UNK_LANG, Common::kPlatformUnknown, "", skipADFlags, skipIncomplete);

	cleanupPirated(matches);

	DetectedGames detectedGames;
	for (uint i = 0; i < matches.size(); i++) {
		DetectedGame game = toDetectedGame(matches[i]);

		if (game.hasUnknownFiles && !(_flags & kADFlagCanPlayUnknownVariants)) {
			game.canBeAdded = false;
		}

		detectedGames.push_back(game);
	}

	bool foundKnownGames = false;
	for (uint i = 0; i < detectedGames.size(); i++) {
		foundKnownGames |= !detectedGames[i].hasUnknownFiles;
	}

	if (!foundKnownGames) {
		// Use fallback detector if there were no matches by other means
		ADDetectedGameExtraInfo *extraInfo = nullptr;
		ADDetectedGame fallbackDetectionResult = fallbackDetect(allFiles, fslist, &extraInfo);

		if (fallbackDetectionResult.desc) {
			DetectedGame fallbackDetectedGame = toDetectedGame(fallbackDetectionResult, extraInfo);

			if (extraInfo != nullptr) {
				// then it's our duty to free it
				delete extraInfo;
			} else {
				// don't add fallback when we are specifying the targetID
				fallbackDetectedGame.preferredTarget += "-fallback";
			}

			if (_flags & kADFlagPreferFallbackDetection) {
				// fallback detection was successful and this engine prefers
				// it over the partial matches found in the detection table
				detectedGames.clear();
			}
			detectedGames.push_back(fallbackDetectedGame);
		}
	}

	return detectedGames;
}

const ExtraGuiOptions AdvancedMetaEngine::getExtraGuiOptions(const Common::String &target) const {
	const ADExtraGuiOptionsMap *extraGuiOptions = getAdvancedExtraGuiOptions();
	if (!extraGuiOptions)
		return ExtraGuiOptions();

	ExtraGuiOptions options;

	// If there isn't any target specified, return all available GUI options.
	// Only used when an engine starts in order to set option defaults.
	if (target.empty()) {
		for (const ADExtraGuiOptionsMap *entry = extraGuiOptions; entry->guioFlag; ++entry)
			options.push_back(entry->option);

		return options;
	}

	// Query the GUI options
	const Common::String guiOptionsString = ConfMan.get("guioptions", target);
	const Common::String guiOptions = parseGameGUIOptions(guiOptionsString);

	// Add all the applying extra GUI options.
	for (const ADExtraGuiOptionsMap *entry = extraGuiOptions; entry->guioFlag; ++entry) {
		if (guiOptions.contains(entry->guioFlag))
			options.push_back(entry->option);
	}

	return options;
}

Common::Error AdvancedMetaEngineDetection::createInstance(OSystem *syst, Engine **engine) {
	assert(engine);

	Common::Language language = Common::UNK_LANG;
	Common::Platform platform = Common::kPlatformUnknown;
	Common::String extra;

	if (ConfMan.hasKey("language"))
		language = Common::parseLanguage(ConfMan.get("language"));
	if (ConfMan.hasKey("platform"))
		platform = Common::parsePlatform(ConfMan.get("platform"));
	if (_flags & kADFlagUseExtraAsHint) {
		if (ConfMan.hasKey("extra"))
			extra = ConfMan.get("extra");
	}

	Common::String gameid = ConfMan.get("gameid");

	Common::String path;
	if (ConfMan.hasKey("path")) {
		path = ConfMan.get("path");
	} else {
		path = ".";
		warning("No path was provided. Assuming the data files are in the current directory");
	}
	Common::FSNode dir(path);
	Common::FSList files;
	if (!dir.isDirectory() || !dir.getChildren(files, Common::FSNode::kListAll)) {
		warning("Game data path does not exist or is not a directory (%s)", path.c_str());
		return Common::kNoGameDataFoundError;
	}

	if (files.empty())
		return Common::kNoGameDataFoundError;

	// Sometimes this method is called directly, so we have to build the maps, especially
	// the _directoryGlobsMap
	preprocessDescriptions();

	// Compose a hashmap of all files in fslist.
	FileMap allFiles;
	composeFileHashMap(allFiles, files, (_maxScanDepth == 0 ? 1 : _maxScanDepth));

	// Clear md5 cache before each detection starts, just in case.
	MD5Man.clear();

	// Run the detector on this
	ADDetectedGames matches = detectGame(files.begin()->getParent(), allFiles, language, platform, extra);

	if (cleanupPirated(matches))
		return Common::kNoGameDataFoundError;

	ADDetectedGame agdDesc;
	for (uint i = 0; i < matches.size(); i++) {
		if (matches[i].desc->gameId == gameid && (!matches[i].hasUnknownFiles || (_flags & kADFlagCanPlayUnknownVariants))) {
			agdDesc = matches[i];
			break;
		}
	}

	if (!agdDesc.desc) {
		// Use fallback detector if there were no matches by other means
		ADDetectedGame fallbackDetectedGame = fallbackDetect(allFiles, files);
		agdDesc = fallbackDetectedGame;
		if (agdDesc.desc) {
			// Seems we found a fallback match. But first perform a basic
			// sanity check: the gameid must match.
			if (agdDesc.desc->gameId != gameid)
				agdDesc = ADDetectedGame();
		}
	}

	if (!agdDesc.desc)
		return Common::kNoGameDataFoundError;

	DetectedGame gameDescriptor = toDetectedGame(agdDesc);

	// If the GUI options were updated, we catch this here and update them in the users config
	// file transparently.
	ConfMan.setAndFlush("guioptions", gameDescriptor.getGUIOptions());

	bool showTestingWarning = false;

#ifdef RELEASE_BUILD
	showTestingWarning = true;
#endif

	if (((gameDescriptor.gameSupportLevel == kUnstableGame
			|| (gameDescriptor.gameSupportLevel == kTestingGame
					&& showTestingWarning)))
			&& !Engine::warnUserAboutUnsupportedGame())
		return Common::kUserCanceled;

	if (gameDescriptor.gameSupportLevel == kWarningGame
			&& !Engine::warnUserAboutUnsupportedGame(gameDescriptor.extra))
		return Common::kUserCanceled;

	if (gameDescriptor.gameSupportLevel == kUnsupportedGame) {
		Engine::errorUnsupportedGame(gameDescriptor.extra);
		return Common::kUserCanceled;
	}

	debug("Running %s", gameDescriptor.description.c_str());
	Common::StringArray filenames;
	for (FilePropertiesMap::const_iterator i = gameDescriptor.matchedFiles.begin(); i != gameDescriptor.matchedFiles.end(); ++i) {
		filenames.push_back(i->_key);
	}
	Common::sort(filenames.begin(), filenames.end());
	for (uint i = 0; i < filenames.size(); ++i) {
		const FileProperties &file = gameDescriptor.matchedFiles[filenames[i]];
		debug("%s: %s, %llu bytes.", filenames[i].c_str(), file.md5.c_str(), (unsigned long long)file.size);
	}
	initSubSystems(agdDesc.desc);

	PluginList pl = EngineMan.getPlugins(PLUGIN_TYPE_ENGINE);
	Plugin *plugin = nullptr;

	// By this point of time, we should have only one plugin in memory.
	if (pl.size() == 1) {
		plugin = pl[0];
	}

	if (plugin) {
		if (_flags & kADFlagMatchFullPaths) {
			Common::StringArray dirs = getPathsFromEntry(agdDesc.desc);
			Common::FSNode gameDataDir = Common::FSNode(ConfMan.get("path"));

			for (auto d = dirs.begin(); d != dirs.end(); ++d)
				SearchMan.addSubDirectoryMatching(gameDataDir, *d, 0, _fullPathGlobsDepth);
		}

		// Call child class's createInstanceMethod.
		return plugin->get<AdvancedMetaEngine>().createInstance(syst, engine, agdDesc.desc);
	}

	return Common::Error(Common::kEnginePluginNotFound);
}

void AdvancedMetaEngineDetection::composeFileHashMap(FileMap &allFiles, const Common::FSList &fslist, int depth, const Common::String &parentName) const {
	if (depth <= 0)
		return;

	if (fslist.empty())
		return;

	for (Common::FSList::const_iterator file = fslist.begin(); file != fslist.end(); ++file) {
		Common::String efname = Common::punycode_encodefilename(file->getName());
		Common::String tstr = ((_flags & kADFlagMatchFullPaths) && !parentName.empty() ? parentName + "/" : "") + efname;

		if (file->isDirectory()) {
			if (!_globsMap.contains(efname))
				continue;

			Common::FSList files;
			if (!file->getChildren(files, Common::FSNode::kListAll))
				continue;

			composeFileHashMap(allFiles, files, depth - 1, tstr);
			continue;
		}

		// Strip any trailing dot
		if (tstr.lastChar() == '.')
			tstr.deleteLastChar();

		if (efname.lastChar() == '.')
			efname.deleteLastChar();

		debugC(9, kDebugGlobalDetection, "$$ ['%s'] ['%s'] in '%s", tstr.c_str(), efname.c_str(), firstPathComponents(fslist.front().getPath(), '/').c_str());

		allFiles[tstr] = *file;		// Record the presence of this file
		allFiles[efname] = *file;	// ...and its file name
	}
}

/* Singleton Cache Storage for MD5 */

namespace Common {
	DECLARE_SINGLETON(MD5CacheManager);
}


static MD5Properties gameFileToMD5Props(const ADGameFileDescription *fileEntry, uint32 gameFlags) {
	MD5Properties ret = kMD5Head;
	if (fileEntry && fileEntry->md5 && strchr(fileEntry->md5, ':')) {
		const char *ptr;
		for (ptr = fileEntry->md5; *ptr != ':'; ptr++)
			switch (*ptr) {
			case 'r':
				ret = (MD5Properties)(ret | kMD5MacResFork);
				break;
			case 'd':
				ret = (MD5Properties)(ret | kMD5MacDataFork);
				break;
			case 't':
				ret = (MD5Properties)(ret | kMD5Tail);
				break;
			}
		return ret;
	}

	if (gameFlags & ADGF_MACRESFORK) {
		ret = (MD5Properties)(ret | kMD5MacResOrDataFork);
	}

	if (gameFlags & ADGF_TAILMD5) {
		ret = (MD5Properties)(ret | kMD5Tail);
	}

	return ret;
}

const char *md5PropToGameFile(MD5Properties flags) {
	switch (flags & kMD5MacMask)
	case kMD5MacDataFork: {
		if (flags & kMD5Tail)
			return "dt";
		return "d";

	case kMD5MacResOrDataFork:
		if (flags & kMD5Tail)
			return "mt";
		return "m";

	case kMD5MacResFork:
		if (flags & kMD5Tail)
			return "rt";
		return "r";

	case kMD5Tail:
		return "t";

	default:
		return "";
	}
}

static bool getFilePropertiesIntern(uint md5Bytes, const AdvancedMetaEngine::FileMap &allFiles, MD5Properties md5prop, const Common::String &fname, FileProperties &fileProps);

bool AdvancedMetaEngineDetection::getFileProperties(const FileMap &allFiles, MD5Properties md5prop, const Common::String &fname, FileProperties &fileProps) const {
	Common::String hashname = Common::String::format("%s:%s:%d", md5PropToCachePrefix(md5prop), fname.c_str(), _md5Bytes);

	if (MD5Man.contains(hashname)) {
		fileProps.md5 = MD5Man.getMD5(hashname);
		fileProps.size = MD5Man.getSize(hashname);
		return true;
	}

	bool res = getFilePropertiesIntern(_md5Bytes, allFiles, md5prop, fname, fileProps);

	if (res) {
		MD5Man.setMD5(hashname, fileProps.md5);
		MD5Man.setSize(hashname, fileProps.size);
	}

	return res;
}

bool AdvancedMetaEngine::getFilePropertiesExtern(uint md5Bytes, const FileMap &allFiles, MD5Properties md5prop, const Common::String &fname, FileProperties &fileProps) const {
	return getFilePropertiesIntern(md5Bytes, allFiles, md5prop, fname, fileProps);
}

static bool getFilePropertiesIntern(uint md5Bytes, const AdvancedMetaEngine::FileMap &allFiles, MD5Properties md5prop, const Common::String &fname, FileProperties &fileProps) {
	if (md5prop & (kMD5MacResFork | kMD5MacDataFork)) {
		FileMapArchive fileMapArchive(allFiles);
		bool is_legacy = ((md5prop & kMD5MacMask) == kMD5MacResOrDataFork);
		if (md5prop & kMD5MacResFork) {
			Common::MacResManager macResMan;

			if (!macResMan.open(fname, fileMapArchive))
				return false;

			fileProps.md5 = macResMan.computeResForkMD5AsString(md5Bytes, ((md5prop & kMD5Tail) != 0));
			fileProps.size = macResMan.getResForkDataSize();

			if (fileProps.size != 0) {
				fileProps.md5prop = (MD5Properties)((md5prop & kMD5Tail) | kMD5MacResFork);
				return true;
			}
		}

		if (md5prop & kMD5MacDataFork) {
			Common::SeekableReadStream *dataFork = Common::MacResManager::openFileOrDataFork(fname, fileMapArchive);
			// Logically 0-sized data fork is valid but legacy code continues fallback
			if (dataFork && (dataFork->size() || !is_legacy)) {
				fileProps.size = dataFork->size();
				fileProps.md5 = Common::computeStreamMD5AsString(*dataFork, md5Bytes);
				fileProps.md5prop = (MD5Properties)((md5prop & kMD5Tail) | kMD5MacDataFork);
				delete dataFork;
				return true;
			}
			delete dataFork;
		}

		// In modern case stop here
		if (!is_legacy)
			return false;
	}

	if (!allFiles.contains(fname))
		return false;

	Common::File testFile;

	if (!testFile.open(allFiles[fname]))
		return false;

	if (md5prop & kMD5Tail) {
		if (testFile.size() > md5Bytes)
			testFile.seek(-(int64)md5Bytes, SEEK_END);
	}

	fileProps.size = testFile.size();
	fileProps.md5 = Common::computeStreamMD5AsString(testFile, md5Bytes);
	fileProps.md5prop = (MD5Properties) (md5prop & kMD5Tail);
	return true;
}

// Add backslash before double quotes (") and backslashes themselves (\)
Common::String escapeString(const char *string) {
	if (string == nullptr || Common::String(string) == "")
		return "NULL";

	Common::String res = "";

	for (int i = 0; string[i] != '\0'; i++) {
		if (string[i] == '"' || string[i] == '\\')
			res += "\\";

		res += string[i];
	}

	return res;
}

void AdvancedMetaEngineDetection::dumpDetectionEntries() const {
	const byte *descPtr;

	for (descPtr = _gameDescriptors; ((const ADGameDescription *)descPtr)->gameId != nullptr; descPtr += _descItemSize) {
		auto g = ((const ADGameDescription *)descPtr);
		const char *title = ((const PlainGameDescriptor *)_gameIds)->description;

		printf("game (\n");
		printf("\tname %s\n", escapeString(g->gameId).c_str());
		printf("\ttitle %s\n", escapeString(title).c_str());
		printf("\textra %s\n", escapeString(g->extra).c_str());
		printf("\tlanguage %s\n", escapeString(getLanguageLocale(g->language)).c_str());
		printf("\tplatform %s\n", escapeString(getPlatformCode(g->platform)).c_str());
		printf("\tsourcefile %s\n", escapeString(getName()).c_str());

		for (auto fileDesc = g->filesDescriptions; fileDesc->fileName; fileDesc++) {
			const char *fname = fileDesc->fileName;
			int64 fsize = fileDesc->fileSize;
			Common::String md5 = fileDesc->md5;
			MD5Properties md5prop = gameFileToMD5Props(fileDesc, g->flags);
			Common::String md5Prefix = md5PropToGameFile(md5prop);
			Common::String key = md5;
			if (md5Prefix != "" && md5.find(':') == Common::String::npos)
				key = md5Prefix + md5;

			printf("\trom ( name \"%s\" size %lld md5-%d %s )\n", escapeString(fname).c_str(), static_cast<long long int>(fsize), _md5Bytes, key.c_str());
		}
		printf(")\n\n"); // Closing for 'game ('
	}
}

ADDetectedGames AdvancedMetaEngineDetection::detectGame(const Common::FSNode &parent, const FileMap &allFiles, Common::Language language, Common::Platform platform, const Common::String &extra, uint32 skipADFlags, bool skipIncomplete) {
	FilePropertiesMap filesProps;
	ADDetectedGames matched;

	const ADGameFileDescription *fileDesc;
	const ADGameDescription *g;
	const byte *descPtr;

	debugC(3, kDebugGlobalDetection, "Starting detection for engine '%s' in dir '%s'", getName(), parent.getPath().c_str());

	preprocessDescriptions();

	// Check which files are included in some ADGameDescription *and* whether
	// they are present. Compute MD5s and file sizes for the available files.
	for (descPtr = _gameDescriptors; ((const ADGameDescription *)descPtr)->gameId != nullptr; descPtr += _descItemSize) {
		g = (const ADGameDescription *)descPtr;

		for (fileDesc = g->filesDescriptions; fileDesc->fileName; fileDesc++) {
			MD5Properties md5prop = gameFileToMD5Props(fileDesc, g->flags);
			Common::String fname = fileDesc->fileName;
			Common::String key = Common::String::format("%s:%s", md5PropToCachePrefix(md5prop), fname.c_str());

			if (filesProps.contains(key))
				continue;

			FileProperties tmp;
			if (getFileProperties(allFiles, md5prop, fname, tmp)) {
				debugC(3, kDebugGlobalDetection, "> '%s': '%s' %ld", key.c_str(), tmp.md5.c_str(), long(tmp.size));
			}

			// Both positive and negative results are cached to avoid
			// repeatedly checking for files.
			filesProps[key] = tmp;
		}
	}

	int maxFilesMatched = 0;
	bool gotAnyMatchesWithAllFiles = false;

	// MD5 based matching
	uint i;
	for (i = 0, descPtr = _gameDescriptors; ((const ADGameDescription *)descPtr)->gameId != nullptr; descPtr += _descItemSize, ++i) {
		g = (const ADGameDescription *)descPtr;

		// Do not even bother to look at entries which do not have matching
		// language and platform (if specified).
		if ((language != Common::UNK_LANG && g->language != Common::UNK_LANG && g->language != language
			 && !(language == Common::EN_ANY && (g->flags & ADGF_ADDENGLISH))) ||
			(platform != Common::kPlatformUnknown && g->platform != Common::kPlatformUnknown && g->platform != platform)) {
			continue;
		}

		if ((_flags & kADFlagUseExtraAsHint) && !extra.empty() && g->extra != extra)
			continue;

		if (g->flags & skipADFlags) {
			debugC(3, kDebugGlobalDetection, "Skipping unsupported target for engine '%s' for the following reasons:\n\t'%s'", g->gameId, g->extra);
			continue;
		}

		ADDetectedGame game(g);
		bool allFilesPresent = true;
		int curFilesMatched = 0;

		// Try to match all files for this game
		for (fileDesc = game.desc->filesDescriptions; fileDesc->fileName; fileDesc++) {
			Common::String tstr = fileDesc->fileName;
			MD5Properties md5prop = gameFileToMD5Props(fileDesc, g->flags);
			Common::String key = Common::String::format("%s:%s", md5PropToCachePrefix(md5prop), tstr.c_str());

			if (!filesProps.contains(key) || filesProps[key].size == -1) {
				allFilesPresent = false;
				break;
			}

			game.matchedFiles[tstr] = filesProps[key];

			if (game.hasUnknownFiles)
				continue;

			const char *md5_wo_prefix = fileDesc->md5;
			if (md5_wo_prefix && strchr(md5_wo_prefix, ':'))
				md5_wo_prefix = strchr(md5_wo_prefix, ':') + 1;

			if (md5_wo_prefix != nullptr && md5_wo_prefix != filesProps[key].md5) {
				debugC(3, kDebugGlobalDetection, "MD5 Mismatch. Skipping (%s) (%s)", fileDesc->md5, filesProps[key].md5.c_str());
				game.hasUnknownFiles = true;
				continue;
			}

			if (fileDesc->fileSize != -1 && fileDesc->fileSize != filesProps[key].size) {
				debugC(3, kDebugGlobalDetection, "Size Mismatch. Skipping (%ld) (%ld)", long(fileDesc->fileSize), long(filesProps[key].size));
				game.hasUnknownFiles = true;
				continue;
			}

			debugC(3, kDebugGlobalDetection, "Matched file: %s", tstr.c_str());
			curFilesMatched++;
		}

		// We found at least one entry with all required files present.
		// That means that we got new variant of the game.
		//
		// Without this check we would have erroneous checksum display
		// where only located files will be enlisted.
		//
		// Potentially this could rule out variants where some particular file
		// is really missing, but the developers should better know about such
		// cases.
		if (allFilesPresent && !gotAnyMatchesWithAllFiles) {
			// Do sanity check
			if (game.hasUnknownFiles && (skipIncomplete || isEntryGrayListed(g))) {
				debugC(3, kDebugGlobalDetection, "Skipping game: %s (%s %s/%s) (%d), %s %s", g->gameId, g->extra, getPlatformDescription(g->platform),
					getLanguageDescription(g->language), i, skipIncomplete ? "(ignore incomplete matches)" : "", isEntryGrayListed(g) ? "(didn't pass sanity test)" : "");
				continue;
			}

			if (matched.empty() || strcmp(matched.back().desc->gameId, g->gameId) != 0)
				matched.push_back(game);
		}

		if (allFilesPresent && !game.hasUnknownFiles) {
			debugC(2, kDebugGlobalDetection, "Found game: %s (%s %s/%s) (%d)", g->gameId, g->extra,
			 getPlatformDescription(g->platform), getLanguageDescription(g->language), i);

			if (curFilesMatched > maxFilesMatched) {
				debugC(2, kDebugGlobalDetection, " ... new best match, removing all previous candidates");
				maxFilesMatched = curFilesMatched;

				matched.clear();	// Remove any prior, lower ranked matches.
				matched.push_back(game);
			} else if (curFilesMatched == maxFilesMatched) {
				matched.push_back(game);
			} else {
				debugC(2, kDebugGlobalDetection, " ... skipped");
			}

			gotAnyMatchesWithAllFiles = true;
		} else {
			debugC(7, kDebugGlobalDetection, "Skipping game: %s (%s %s/%s) (%d)", g->gameId, g->extra,
			 getPlatformDescription(g->platform), getLanguageDescription(g->language), i);
		}
	}

	debugC(2, "Totally found %d matches", matched.size());

	return matched;
}

ADDetectedGame AdvancedMetaEngineDetection::detectGameFilebased(const FileMap &allFiles, const ADFileBasedFallback *fileBasedFallback) const {
	const ADFileBasedFallback *ptr;
	const char* const* filenames;

	int maxNumMatchedFiles = 0;
	ADDetectedGame result;

	for (ptr = fileBasedFallback; ptr->desc; ++ptr) {
		const ADGameDescription *agdesc = ptr->desc;
		int numMatchedFiles = 0;
		bool fileMissing = false;

		for (filenames = ptr->filenames; *filenames; ++filenames) {
			debugC(3, kDebugGlobalDetection, "++ %s", *filenames);
			if (!allFiles.contains(*filenames)) {
				fileMissing = true;
				break;
			}

			numMatchedFiles++;
		}

		if (!fileMissing) {
			debugC(4, kDebugGlobalDetection, "Matched: %s", agdesc->gameId);

			if (numMatchedFiles > maxNumMatchedFiles) {
				MD5Properties md5prop = gameFileToMD5Props(nullptr, agdesc->flags);
				maxNumMatchedFiles = numMatchedFiles;

				debugC(4, kDebugGlobalDetection, "and overridden");

				ADDetectedGame game(agdesc);
				game.hasUnknownFiles = true;

				for (filenames = ptr->filenames; *filenames; ++filenames) {
					FileProperties tmp;

					if (getFileProperties(allFiles, md5prop, *filenames, tmp))
						game.matchedFiles[*filenames] = tmp;
				}

				result = game;
			}
		}
	}

	return result;
}

PlainGameList AdvancedMetaEngineDetection::getSupportedGames() const {
	return PlainGameList(_gameIds);
}

PlainGameDescriptor AdvancedMetaEngineDetection::findGame(const char *gameId) const {
	// First search the list of supported gameids for a match.
	const PlainGameDescriptor *g = findPlainGameDescriptor(gameId, _gameIds);
	if (g)
		return *g;

	// No match found
	return PlainGameDescriptor::empty();
}

static const char *grayList[] = {
	"game.exe",
	"demo.exe",
	"game",
	"gamedata",
	"demo",
	"data",
	"data.z",
	"data.cab",
	"data1.hdr",
	"data1.cab",
	"data2.cab",
	"data3.cab",
	"data4.cab",
	"data5.cab",
	"engine.exe",
	"install.exe",
	"play.exe",
	"start.exe",
	"item.dat",
	"abc.exe",
	"ppc",
	0
};

AdvancedMetaEngineDetection::AdvancedMetaEngineDetection(const void *descs, uint descItemSize, const PlainGameDescriptor *gameIds)
	: _gameDescriptors((const byte *)descs), _descItemSize(descItemSize), _gameIds(gameIds) {

	_md5Bytes = 5000;
	_flags = 0;
	_guiOptions = GUIO_NONE;
	_maxScanDepth = 1;
	_directoryGlobs = NULL;
	_maxAutogenLength = 15;
	_fullPathGlobsDepth = 5;

	_hashMapsInited = false;

	for (auto f = grayList; *f; f++)
		_grayListMap.setVal(*f, true);
}

void AdvancedMetaEngineDetection::initSubSystems(const ADGameDescription *gameDesc) const {
#ifdef ENABLE_EVENTRECORDER
	if (gameDesc) {
		g_eventRec.processGameDescription(gameDesc);
	}
#endif
}

void AdvancedMetaEngineDetection::preprocessDescriptions() {
	if (_hashMapsInited)
		return;

	_hashMapsInited = true;

	// Put all directory globs into a hashmap for faster usage
	if (_directoryGlobs) {
		for (auto glob = _directoryGlobs; *glob; glob++)
			_globsMap.setVal(*glob, true);
	}

	// Now scan all detection entries
	for (const byte *descPtr = _gameDescriptors; ((const ADGameDescription *)descPtr)->gameId != nullptr; descPtr += _descItemSize) {
		const ADGameDescription *g = (const ADGameDescription *)descPtr;

		// Scan for potential directory globs
		for (const ADGameFileDescription *fileDesc = g->filesDescriptions; fileDesc->fileName; fileDesc++) {
			if (strchr(fileDesc->fileName, '/')) {
				if (!(_flags & kADFlagMatchFullPaths))
					warning("Path component detected in entry for '%s' in engine '%s' but no kADFlagMatchFullPaths is set",
						g->gameId, getName());

				Common::StringTokenizer tok(fileDesc->fileName, "/");

				uint32 depth = 0;

				while (!tok.empty()) {
					Common::String component = tok.nextToken();

					if (!tok.empty() && !_globsMap.contains(component.c_str())) { // If it is not the last component
						_globsMap.setVal(component, true);
						debugC(4, kDebugGlobalDetection, "  Added '%s' to globs", component.c_str());
					}

					depth++;
				}

				if (depth > _maxScanDepth) {
					_maxScanDepth = depth;

					debugC(4, kDebugGlobalDetection, "  Increased scan depth to %d", _maxScanDepth);
				}
			}
		}

		// Check if the detection entry have only files from the blacklist
		if (isEntryGrayListed(g)) {
			debug(0, "WARNING: Detection entry for '%s' in engine '%s' contains only blacklisted names. Add more files to the entry (%s)",
				g->gameId, getName(), g->filesDescriptions[0].md5);
		}
	}

#ifndef RELEASE_BUILD
	// Check the provided tables for sanity
	detectClashes();
#endif
}

Common::StringArray AdvancedMetaEngineDetection::getPathsFromEntry(const ADGameDescription *g) {
	Common::StringArray result;
	Common::HashMap<Common::String, bool, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> unique;

	for (const ADGameFileDescription *fileDesc = g->filesDescriptions; fileDesc->fileName; fileDesc++) {
		if (!strchr(fileDesc->fileName, '/'))
			continue;

		Common::StringTokenizer tok(fileDesc->fileName, "/");

		while (!tok.empty()) {
			Common::String component = tok.nextToken();

			if (!tok.empty()) { // If it is not the last component
				if (!unique.contains(component))
					result.push_back(component);

				unique[component] = true;
			}
		}
	}

	return result;
}

bool AdvancedMetaEngineDetection::isEntryGrayListed(const ADGameDescription *g) const {
	bool grayIsPresent = false, nonGrayIsPresent = false;

	for (const ADGameFileDescription *fileDesc = g->filesDescriptions; fileDesc->fileName; fileDesc++) {
		if (_grayListMap.contains(fileDesc->fileName)) {
			grayIsPresent = true;
		} else {
			nonGrayIsPresent = true;
		}
	}

	return (grayIsPresent && !nonGrayIsPresent);
}

Common::Error AdvancedMetaEngine::createInstance(OSystem *syst, Engine **engine) {
	PluginList pl = PluginMan.getPlugins(PLUGIN_TYPE_ENGINE);
	if (pl.size() == 1) {
		const Plugin *metaEnginePlugin = PluginMan.getMetaEngineFromEngine(pl[0]);
		if (metaEnginePlugin) {
			return metaEnginePlugin->get<AdvancedMetaEngineDetection>().createInstance(syst, engine);
		}
	}

	return Common::Error();
}

void AdvancedMetaEngineDetection::detectClashes() const {
	// First, check that we do not have duplicated entries in _gameIds
	Common::HashMap<Common::String, int, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> idsMap;


	for (const PlainGameDescriptor *g = _gameIds; g->gameId; g++) {
		if (idsMap.contains(g->gameId))
			debug(0, "WARNING: Detection gameId for '%s' in engine '%s' has duplicates", g->gameId, getName());

		idsMap[g->gameId] = 0;
	}

	for (const byte *descPtr = _gameDescriptors; ((const ADGameDescription *)descPtr)->gameId != nullptr; descPtr += _descItemSize) {
		const ADGameDescription *g = (const ADGameDescription *)descPtr;

		if (!idsMap.contains(g->gameId)) {
			debug(0, "WARNING: Detection gameId for '%s' in engine '%s' is not present in gameids", g->gameId, getName());
		} else {
			idsMap[g->gameId]++;
		}
	}

	for (auto &k : idsMap) {
		if (k._value == 0 && k._key != getName())
			debug(0, "WARNING: Detection gameId for '%s' in engine '%s' has no games in the detection table", k._key.c_str(), getName());
	}
}

bool AdvancedMetaEngine::checkExtendedSaves(MetaEngineFeature f) const {
	return (f == kSavesUseExtendedFormat) ||
		(f == kSimpleSavesNames) ||
		(f == kSupportsListSaves) ||
		(f == kSupportsDeleteSave) ||
		(f == kSavesSupportMetaInfo) ||
		(f == kSavesSupportThumbnail) ||
		(f == kSavesSupportCreationDate) ||
		(f == kSavesSupportPlayTime);
}
