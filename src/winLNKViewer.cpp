// Copyright 2007 Matthew A. Kucenski
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//#define _DEBUG_

#include "libWinLNK/src/winLnkFile.h"

#include <popt.h>

#include <string>
#include <vector>
#include <iostream>
using namespace std;

#include "libtimeUtils/src/timeUtils.h"
#include "libtimeUtils/src/timeZoneCalculator.h"
#include "misc/poptUtils.h"
#include "misc/debugMsgs.h"

int main(int argc, const char** argv) {
	int rv = EXIT_FAILURE;
	
	vector<string> filenameVector;
	bool bDelimited = false;
	bool bMactime = false;
	timeZoneCalculator tzcalc;
	
	struct poptOption optionsTable[] = {
		{"delimited",	'd',	POPT_ARG_NONE,		NULL, 10,	"Display in comma-delimited format.", NULL},
		{"mactime",		'm',	POPT_ARG_NONE,		NULL,	20,	"Display in the SleuthKit's mactime format (TSK 3.0+).", NULL},
		{"timezone", 	'z',	POPT_ARG_STRING,	NULL,	30,	"POSIX timezone string (e.g. 'EST-5EDT,M4.1.0,M10.1.0' or 'GMT-5') to be used when displaying data. Defaults to GMT.", "zone"},
		{"version", 	0,		POPT_ARG_NONE,		NULL,	100,	"Display version.", NULL},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	poptContext optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
	poptSetOtherOptionHelp(optCon, "[options] <filename> [<filename>] ...");
	
	if (argc < 2) {
		poptPrintUsage(optCon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	string strTmp;
	int iOption = poptGetNextOpt(optCon);
	while (iOption >= 0) {
		switch (iOption) {
			case 10:
				bDelimited = true;
				break;
			case 20:
				bMactime = true;
				break;
			case 30:
				if (tzcalc.setTimeZone(poptGetOptArg(optCon)) >= 0) {
				} else {
					usage(optCon, "Invalid time zone string", "e.g. 'EST-5EDT,M4.1.0,M10.1.0' or 'GMT-5'");
					exit(EXIT_FAILURE);
				}
				break;
			case 100:
				version(PACKAGE, VERSION);
				exit(EXIT_SUCCESS);
				break;
		}
		iOption = poptGetNextOpt(optCon);
	}
	
	if (iOption != -1) {
		usage(optCon, poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(iOption));
		exit(EXIT_FAILURE);
	}
	
	const char* cstrFilename = poptGetArg(optCon);
	while (cstrFilename) {
		filenameVector.push_back(cstrFilename);
		cstrFilename = poptGetArg(optCon);
	}
	
	if (filenameVector.size() < 1) {
		usage(optCon, "You must specify at least one file", "e.g., shortcut.lnk");
		exit(EXIT_FAILURE);
	}
	
	if (bDelimited) {
		cout << "Shortcut,Path,Size,\"Created (" << tzcalc.getTimeZoneString() << ")\",\"Modified (" << tzcalc.getTimeZoneString() << ")\",\"Accessed (" << tzcalc.getTimeZoneString() << ")\",Local Path,Local Type,Local Label,Local Serial #,Network Share,Description,Relative Path,Working Directory,Command Line Args" << endl;
	} //if (bDelimited) {
	
	for (vector<string>::iterator it = filenameVector.begin(); it != filenameVector.end(); it++) {
		winLnkFile lnkFile(*it);
		
		if (lnkFile.load() < 0) {
			cerr << "Warning! Errors reading shortcut file (" << *it << ").\n";
		}

		//Get posix_time::ptime values from the Windows64 time values stored in the lnk file.
		posix_time::ptime psxCreatedTime = getFromWindows64(lnkFile.getCreatedTime());
		posix_time::ptime psxModifiedTime = getFromWindows64(lnkFile.getModifiedTime());
		posix_time::ptime psxAccessedTime = getFromWindows64(lnkFile.getAccessedTime());
		
		DEBUG_INFO(psxCreatedTime << "/" << psxModifiedTime << "/" << psxAccessedTime);
		
		//Using the given time zone, calculate the local times
		local_time::local_date_time ldtCreatedTime = tzcalc.calculateLocalTime(psxCreatedTime);
		local_time::local_date_time ldtModifiedTime = tzcalc.calculateLocalTime(psxModifiedTime);
		local_time::local_date_time ldtAccessedTime = tzcalc.calculateLocalTime(psxAccessedTime);
		
		//Convert the time values into strings
		string strCreatedTime = getDateString(ldtCreatedTime) + " " + getTimeString(ldtCreatedTime);
		string strModifiedTime = getDateString(ldtModifiedTime) + " " + getTimeString(ldtModifiedTime);
		string strAccessedTime = getDateString(ldtAccessedTime) + " " + getTimeString(ldtAccessedTime);

		if (bMactime) {
			//TSK 3.0+: MD5|name|inode|mode_as_string|UID|GID|size|atime|mtime|ctime|crtime

		string pathname;
		if (lnkFile.isAvailableLocal()) {
			pathname = lnkFile.getBasePath();
		} else {
			if (lnkFile.isAvailableNetwork()) {
				pathname = lnkFile.getNetworkShare() + "\\" + lnkFile.getFinalPath();
			}
		}

			cout 	<< "0|" 																		//MD5
					<< *it << " -> " << pathname << "|"									//name
					<< "0|"																		//inode
					<< "lnk---------|"														//mode_as_string
					<< "0|"																		//UID
					<< "0|"																		//GID
					<< lnkFile.getFileLength() << "|" 									//size
					<< getUnix32FromWindows64(lnkFile.getAccessedTime()) << "|" //atime
					<< getUnix32FromWindows64(lnkFile.getModifiedTime()) << "|" //mtime
					<< getUnix32FromWindows64(lnkFile.getModifiedTime()) << "|"	//ctime
					<< getUnix32FromWindows64(lnkFile.getCreatedTime()) 			//crtime
					<< "\n";
		} else {
			if (bDelimited) {
				cout 	<< "\"" << *it << "\","
						<< "\"" << lnkFile.getFinalPath() << "\","
						<< lnkFile.getFileLength() << ","
						<< strCreatedTime << ","
						<< strModifiedTime << ","
						<< strAccessedTime << ","
						<< "\"" << lnkFile.getBasePath() << "\",";
				u_int32_t uiLocalVolType = lnkFile.getLocalVolumeType();
				cout << "\"" << ( uiLocalVolType == 0 ? "Unknown" : 
									(uiLocalVolType == 1 ? "None" : 
										(uiLocalVolType == 2 ? "Removeable" : 
											(uiLocalVolType == 3 ? "Fixed" : 
												(uiLocalVolType == 4 ? "Remote" : 
													(uiLocalVolType == 5 ? "CD-ROM" : 
														(uiLocalVolType == 6 ? "RAM Drive" : 
															"Invalid"))))))) << "\",";
				cout 	<< "\"" << lnkFile.getLocalVolumeName() << "\","
						<< lnkFile.getLocalVolumeSerialString() << ","
						<< "\"" << lnkFile.getNetworkShare() << "\",";
				STROUT 	<< STR("\"") << lnkFile.getDescription() << STR("\",")
						<< STR("\"") << lnkFile.getRelativePath() << STR("\",")
						<< STR("\"") << lnkFile.getWorkingDir() << STR("\",")
						<< STR("\"") << lnkFile.getCommandLine() << STR("\",");
				cout << "\n";
			} else {	//if (bDelimited) {
				cout << "Shortcut: " << *it << "\n";
				cout << "\n";
				
				cout << "Target:\n";
				cout << "   Local:            " << (lnkFile.isAvailableLocal() ? "X" : "-") << "\n";
				cout << "   Network:          " << (lnkFile.isAvailableNetwork() ? "X" : "-") << "\n";
				cout << "   Path:             " << lnkFile.getFinalPath() << "\n";
				cout << "   Size:             " << lnkFile.getFileLength() << " bytes\n";
				cout << "   Created:          " << strCreatedTime << " (" << tzcalc.getTimeZoneString() << ")\n";
				cout << "   Modified:         " << strModifiedTime << " (" << tzcalc.getTimeZoneString() << ")\n";
				cout << "   Accessed:         " << strAccessedTime << " (" << tzcalc.getTimeZoneString() << ")\n";
				cout << "   Local Volume:\n";
				cout << "      Path:          " << lnkFile.getBasePath() << "\n";
				u_int32_t uiLocalVolType = lnkFile.getLocalVolumeType();
				cout << "      Type:          " << ( uiLocalVolType == 0 ? "Unknown" : 
														(uiLocalVolType == 1 ? "None" : 
															(uiLocalVolType == 2 ? "Removeable" : 
																(uiLocalVolType == 3 ? "Fixed" : 
																	(uiLocalVolType == 4 ? "Remote" : 
																		(uiLocalVolType == 5 ? "CD-ROM" : 
																			(uiLocalVolType == 6 ? "RAM Drive" : 
																				"Invalid"))))))) << "\n";
				cout << "      Label:         " << lnkFile.getLocalVolumeName() << "\n";
				cout << "      Serial Number: " << lnkFile.getLocalVolumeSerialString() << "\n";
				cout << "   Network Volume:\n";
				cout << "      Share Name:    " << lnkFile.getNetworkShare() << "\n";
				cout << "\n";
				
				cout << "Attributes:\n";
				cout << "   Read Only:    " << (lnkFile.isReadOnly() ? "X" : "-") << "\n";
				cout << "   Hidden:       " << (lnkFile.isHidden() ? "X" : "-") << "\n";
				cout << "   System:       " << (lnkFile.isSystem() ? "X" : "-") << "\n";
				cout << "   Volume Label: " << (lnkFile.isVolumeLabel() ? "X" : "-") << "\n";
				cout << "   Directory:    " << (lnkFile.isDirectory() ? "X" : "-") << "\n";
				cout << "   Archive:      " << (lnkFile.isArchive() ? "X" : "-") << "\n";
				cout << "   Encrypted:    " << (lnkFile.isEncrypted() ? "X" : "-") << "\n";
				cout << "   Normal:       " << (lnkFile.isNormal() ? "X" : "-") << "\n";
				cout << "   Temporary:    " << (lnkFile.isTemporary() ? "X" : "-") << "\n";
				cout << "   Sparse:       " << (lnkFile.isSparseFile() ? "X" : "-") << "\n";
				cout << "   Reparse Data: " << (lnkFile.hasReparsePointData() ? "X" : "-") << "\n";
				cout << "   Compressed:   " << (lnkFile.isCompressed() ? "X" : "-") << "\n";
				cout << "   Offline:      " << (lnkFile.isOffline() ? "X" : "-") << "\n";
				cout << "\n";
			
				cout  <<  "GUID:              " << lnkFile.getGUID() << "\n";
				cout  <<  "Hot Key:           " << lnkFile.getHotKey() << "\n";
				u_int32_t showWnd = lnkFile.getShowWindowValue();
				cout  <<  "Show Window:       " << (showWnd == 0 ?	"Hide" : 
														(showWnd == 1 ? "Normal" : 
															(showWnd == 2 ? "Show, Minimized" : 
																(showWnd == 3 ? "Show, Maximized" : 
																	(showWnd == 4 ? "Show, No Activate" : 
																		(showWnd == 5 ? "Show" : 
																			(showWnd == 6 ? "Minimized" : 
																				(showWnd == 7 ? "Show, Minimized, No Active" : 
																					(showWnd == 8 ? "Show, No Active" : 
																						(showWnd == 9 ? "Restore" : 
																							(showWnd == 10 ? "Show, Default" : 
																								"Invalid"))))))))))) << " (" << showWnd << ")\n";
				STROUT << STR("Description:       ") << lnkFile.getDescription() << STR("\n");
				STROUT << STR("Relative Path:     ") << lnkFile.getRelativePath() << STR("\n");
				STROUT << STR("Working Directory: ") << lnkFile.getWorkingDir() << STR("\n");
				STROUT << STR("Command Line Args: ") << lnkFile.getCommandLine() << STR("\n");
				STROUT << STR("Custom Icon File:  ") << lnkFile.getIconFilename();
				if (lnkFile.getIconFilename().length() > 0) { 
					cout << " (" << lnkFile.getIconNumber() << ")"; 
				} 
				cout << "\n";
				
				if (filenameVector.size() > 1) {
					cout << "-------------------------------------------------------------------------------\n";
				}
			}	//if (bDelimited) {
		}	//if (bMactime) {
	}	//for (vector<string>::iterator it = filenameVector.begin(); it != filenameVector.end(); it++) {
	
	exit(rv);	
}	//int main(int argc, const char** argv) {
