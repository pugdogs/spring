#include "StdAfx.h"
#include "CobFile.h"
#include "FileHandler.h"
#include "InfoConsole.h"
#include <algorithm>
#include <locale>
#include <cctype>
#include "Sound.h"
#include "byteorder.h"
//#include "mmgr.h"

//The following structure is taken from http://visualta.tauniverse.com/Downloads/ta-cob-fmt.txt
//Information on missing fields from Format_Cob.pas
typedef struct tagCOBHeader
{
	int VersionSignature;
	int NumberOfScripts;
	int NumberOfPieces;
	int TotalScriptLen;
	int NumberOfStaticVars;
	int Unknown_2; /* Always seems to be 0 */
	int OffsetToScriptCodeIndexArray;
	int OffsetToScriptNameOffsetArray;
	int OffsetToPieceNameOffsetArray;
	int OffsetToScriptCode;
	int Unknown_3; /* Always seems to point to first script name */

	int OffsetToSoundNameArray;		// These two are only found in TA:K scripts
	int NumberOfSounds;
} COBHeader;

#define READ_COBHEADER(ch,src)						\
do {									\
	unsigned int __tmp;						\
	unsigned short __isize = sizeof(unsigned int);			\
	unsigned int __c = 0;						\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).VersionSignature = (int)swabdword(__tmp);			\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).NumberOfScripts = (int)swabdword(__tmp);			\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).NumberOfPieces = (int)swabdword(__tmp);			\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).TotalScriptLen = (int)swabdword(__tmp);			\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).NumberOfStaticVars = (int)swabdword(__tmp);		\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).Unknown_2 = (int)swabdword(__tmp);				\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).OffsetToScriptCodeIndexArray = (int)swabdword(__tmp);	\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).OffsetToScriptNameOffsetArray = (int)swabdword(__tmp);	\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).OffsetToPieceNameOffsetArray = (int)swabdword(__tmp);	\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).OffsetToScriptCode = (int)swabdword(__tmp);		\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).Unknown_3 = (int)swabdword(__tmp);				\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).OffsetToSoundNameArray = (int)swabdword(__tmp);		\
	__c+=__isize;							\
	memcpy(&__tmp,&((src)[__c]),__isize);				\
	(ch).NumberOfSounds = (int)swabdword(__tmp);			\
} while (0)


CCobFile::CCobFile(CFileHandler &in, string name)
{
	char *cobdata = NULL;

	this->name = name;

	//Figure out size needed and allocate it
	int size = in.FileSize();

	// Handle errors (this is fairly fatal..)
	if (size < 0) {
		info->AddLine("Could not find script for unit %s", name.c_str());
		exit(0);
	}

	cobdata = new char[size];

	//Read the entire thing, we will need it
	in.Read(cobdata, size);

	//Time to parse
	COBHeader ch;
	READ_COBHEADER(ch,cobdata);

	for (int i = 0; i < ch.NumberOfScripts; ++i) {
		int ofs;
		
		ofs = *(int *)&cobdata[ch.OffsetToScriptNameOffsetArray + i * 4];
		string s = &cobdata[ofs];
		scriptNames.push_back(s);

		ofs = *(int *)&cobdata[ch.OffsetToScriptCodeIndexArray + i * 4];
		scriptOffsets.push_back(ofs);
	}

	//Check for zero-length scripts
	for (int i = 0; i < ch.NumberOfScripts - 1; ++i) {
		scriptLengths.push_back(scriptOffsets[i + 1] - scriptOffsets[i]);
	}
	scriptLengths.push_back(ch.TotalScriptLen - scriptOffsets[ch.NumberOfScripts - 1]);

	for (int i = 0; i < ch.NumberOfPieces; ++i) {
		int ofs;

		ofs = *(int *)&cobdata[ch.OffsetToPieceNameOffsetArray + i * 4];
		string s = &cobdata[ofs];
		std::transform(s.begin(), s.end(), s.begin(), (int (*)(int))std::tolower);
		pieceNames.push_back(s);
	}

	code = new int[(size - ch.OffsetToScriptCode) / 4 + 4];
	memcpy(code, &cobdata[ch.OffsetToScriptCode], size - ch.OffsetToScriptCode);

	numStaticVars = ch.NumberOfStaticVars;

	// If this is a TA:K script, read the sound names
	if (ch.VersionSignature == 6) {
		for (long i = 0; i < ch.NumberOfSounds; ++i) {
			long ofs;
			ofs = *(long *)&cobdata[ch.OffsetToSoundNameArray + i * 4];
			string s = &cobdata[ofs];

			// Load the wave file and store the ID for future use
			s = s + ".wav";
			sounds.push_back(sound->GetWaveId(s));
		}
	}

	delete[] cobdata;

	//Create a reverse mapping (name->int)
	for (unsigned int i = 0; i < scriptNames.size(); ++i) {
		scriptMap[scriptNames[i]] = i;
	}

	//Map common function names to indicies
	scriptIndex.resize(COBFN_Last + COB_MaxWeapons * 5);
	scriptIndex[COBFN_Create] = getFunctionId("Create");
	scriptIndex[COBFN_StartMoving] = getFunctionId("StartMoving");
	scriptIndex[COBFN_StopMoving] = getFunctionId("StopMoving");
	scriptIndex[COBFN_Activate] = getFunctionId("Activate");
	scriptIndex[COBFN_Killed] = getFunctionId("Killed");
	scriptIndex[COBFN_Deactivate] = getFunctionId("Deactivate");
	scriptIndex[COBFN_SetDirection] = getFunctionId("SetDirection");
	scriptIndex[COBFN_SetSpeed] = getFunctionId("SetSpeed");
	scriptIndex[COBFN_RockUnit] = getFunctionId("RockUnit");
	scriptIndex[COBFN_HitByWeapon] = getFunctionId("HitByWeapon");
	scriptIndex[COBFN_MoveRate0] = getFunctionId("MoveRate0");
	scriptIndex[COBFN_MoveRate1] = getFunctionId("MoveRate1");
	scriptIndex[COBFN_MoveRate2] = getFunctionId("MoveRate2");
	scriptIndex[COBFN_MoveRate3] = getFunctionId("MoveRate3");
	scriptIndex[COBFN_SetSFXOccupy] = getFunctionId("setSFXoccupy");

	// Also add the weapon aiming stuff
	for (int i = 0; i < COB_MaxWeapons; ++i) {
		char buf[15];
		sprintf(buf, "Weapon%d", i + 1);
		string weapon(buf);
		sprintf(buf, "%d", i + 1);
		string weap(buf);
		scriptIndex[COBFN_QueryPrimary + i] = getFunctionId("Query" + weapon);
		scriptIndex[COBFN_AimPrimary + i] = getFunctionId("Aim" + weapon);
		scriptIndex[COBFN_AimFromPrimary + i] = getFunctionId("AimFrom" + weapon);
		scriptIndex[COBFN_FirePrimary + i] = getFunctionId("Fire" + weapon);
		scriptIndex[COBFN_EndBurst + i] = getFunctionId("EndBurst" + weap);

		// If new-naming functions are not found, we need to support the old naming scheme
		if (i > 2)
			continue;
		switch (i) {
			case 0: weapon = "Primary"; break;
			case 1: weapon = "Secondary"; break;
			case 2: weapon = "Tertiary"; break;
		}

		if (scriptIndex[COBFN_QueryPrimary + i] == -1)
			scriptIndex[COBFN_QueryPrimary + i] = getFunctionId("Query" + weapon);
		if (scriptIndex[COBFN_AimPrimary + i] == -1)
			scriptIndex[COBFN_AimPrimary + i] = getFunctionId("Aim" + weapon);
		if (scriptIndex[COBFN_AimFromPrimary + i] == -1)
			scriptIndex[COBFN_AimFromPrimary + i] = getFunctionId("AimFrom" + weapon);
		if (scriptIndex[COBFN_FirePrimary + i] == -1)
			scriptIndex[COBFN_FirePrimary + i] = getFunctionId("Fire" + weapon);
	}
}

CCobFile::~CCobFile(void)
{
	//test
	delete[] code;
}

int CCobFile::getFunctionId(const string &name)
{
	map<string, int>::iterator i;
	if ((i = scriptMap.find(name)) != scriptMap.end()) {
		return i->second;
	}

	return -1;
}

