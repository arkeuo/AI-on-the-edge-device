#ifndef COMPONENTS_HELPER_SDCARD_CHECK_H
#define COMPONENTS_HELPER_SDCARD_CHECK_H


int checkSdCardRW(void);
int checkSdCardSustainedRW(void);
int getSdCardSustainedRWResult(void);
bool checkSdCardFolderFilePresence(void);

#endif // COMPONENTS_HELPER_SDCARD_CHECK_H
