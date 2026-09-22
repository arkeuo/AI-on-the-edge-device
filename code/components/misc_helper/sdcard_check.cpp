#include "sdcard_check.h"
#include "../../include/defines.h"

#include <cstring>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "esp_rom_crc.h"
#include "esp_timer.h"
#include <stdlib.h>

#include "ClassLogFile.h"


static const char *TAG = "SDCARD";

// Sustained R/W check
//
// checkSdCardRW() above writes and reads back 47 bytes. A card that is already
// unusable for large reads still passes that every single time: a real failure
// case logged 309 consecutive successful basic checks while failing every read
// of a 315 kB tflite model, which put the device into a boot loop it never
// diagnosed. The device kept reporting itself healthy throughout.
//
// This check is deliberately larger than the biggest model the device loads, and
// verifies byte by byte rather than by checksum so that a failure reports the
// exact offset where the card starts returning garbage.
#define SDCARD_SUSTAINED_FILE       "/sdcard/sdcheck_sustained.bin"
#define SDCARD_SUSTAINED_SIZE       (512 * 1024)
#define SDCARD_SUSTAINED_CHUNK      (4 * 1024)

static int s_iLastSustainedResult = -99; // -99: not run yet

static int checkSdCardSustainedRWInternal(void);


static inline uint8_t sustainedPatternByte(size_t index)
{
    return (uint8_t)((index * 31u + 7u) & 0xFFu);
}

int checkSdCardRW(void)
{
    LogFile.writeToFile(ESP_LOG_INFO, TAG, "Basic R/W check started");
    FILE *pFile = NULL;
    int iCRCMessage = 0;

    pFile = fopen("/sdcard/sdcheck.txt", "w");
    if (pFile == NULL) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E1) Not able to open file to write");
        return -1;
    }
    else {
        std::string sMessage = "This message is used for a SD-Card basic check!";
        iCRCMessage = esp_rom_crc16_le(0, (uint8_t *)sMessage.c_str(), sMessage.length());
        if (fwrite(sMessage.c_str(), sMessage.length(), 1, pFile) == 0) {
            LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E2) Not able to write file");
            fclose(pFile);
            unlink("/sdcard/sdcheck.txt");
            return -2;
        }
        fclose(pFile);
    }

    pFile = fopen("/sdcard/sdcheck.txt", "r");
    if (pFile == NULL) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E3) Not able to open file to read back");
        unlink("/sdcard/sdcheck.txt");
        return -3;
    }
    else {
        char cReadBuf[50];
        if (fgets(cReadBuf, sizeof(cReadBuf), pFile) == 0) {
            LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E4) Not able to read file back");
            fclose(pFile);
            unlink("/sdcard/sdcheck.txt");
            return -4;
        }
        else {
            if (esp_rom_crc16_le(0, (uint8_t *)cReadBuf, strlen(cReadBuf)) != iCRCMessage) {
                LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E5) Read back, but wrong CRC");
                fclose(pFile);
                unlink("/sdcard/sdcheck.txt");
                return -5;
            }
        }
        fclose(pFile);
    }

    if (unlink("/sdcard/sdcheck.txt") != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Basic R/W check: (E6) Unable to delete the file");
        return -6;
    }

    LogFile.writeToFile(ESP_LOG_INFO, TAG, "Basic R/W check successful");
    return 0;
}


bool checkSdCardFolderFilePresence()
{
    struct stat sb;
    bool bRetval = true;

    LogFile.writeToFile(ESP_LOG_INFO, TAG, "Folder/file presence check started");
    /* check if folder exists: config */
    if (stat("/sdcard/config", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /config not found");
        bRetval = false;
    }

    /* check if folder exists: config/certs */
    if (stat("/sdcard/config/certs", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /config/certs not found");
        bRetval = false;
    }

    /* check if folder exists: config/models */
    if (stat("/sdcard/config/models", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /config/models not found");
        bRetval = false;
    }

    /* check if folder exists: html */
    if (stat("/sdcard/html", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /html not found");
        bRetval = false;
    }

    /* check if folder exists: img_tmp */
    if (stat("/sdcard/img_tmp", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /img_tmp not found");
        bRetval = false;
    }

    /* check if folder exists: log */
    if (stat("/sdcard/log", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /log not found");
        bRetval = false;
    }

    /* check if folder exists: demo */
    if (stat("/sdcard/demo", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: Folder /demo not found");
        bRetval = false;
    }

    /* check if file exists: index.html */
    if (stat("/sdcard/html/index.html", &sb) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Folder/file check: File /html/index.html not found");
        bRetval = false;
    }

    if (bRetval) {
        LogFile.writeToFile(ESP_LOG_INFO, TAG, "Folder/file presence check successful");
    }

    return bRetval;
}


int checkSdCardSustainedRW(void)
{
    s_iLastSustainedResult = checkSdCardSustainedRWInternal();
    return s_iLastSustainedResult;
}


int getSdCardSustainedRWResult(void)
{
    return s_iLastSustainedResult;
}


static int checkSdCardSustainedRWInternal(void)
{
    LogFile.writeToFile(ESP_LOG_INFO, TAG, "Sustained R/W check started (" +
                        std::to_string(SDCARD_SUSTAINED_SIZE / 1024) + " kB)");

    uint8_t *pBuffer = (uint8_t *)malloc(SDCARD_SUSTAINED_CHUNK);
    if (pBuffer == NULL) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E10) Not able to allocate buffer");
        return -10;
    }

    /* Write phase */
    FILE *pFile = fopen(SDCARD_SUSTAINED_FILE, "wb");
    if (pFile == NULL) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E11) Not able to open file to write");
        free(pBuffer);
        return -11;
    }

    int64_t i64Start = esp_timer_get_time();
    size_t szWritten = 0;
    while (szWritten < SDCARD_SUSTAINED_SIZE) {
        for (size_t i = 0; i < SDCARD_SUSTAINED_CHUNK; i++) {
            pBuffer[i] = sustainedPatternByte(szWritten + i);
        }
        if (fwrite(pBuffer, 1, SDCARD_SUSTAINED_CHUNK, pFile) != SDCARD_SUSTAINED_CHUNK) {
            LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E12) Write failed at offset " +
                                std::to_string(szWritten));
            fclose(pFile);
            free(pBuffer);
            unlink(SDCARD_SUSTAINED_FILE);
            return -12;
        }
        szWritten += SDCARD_SUSTAINED_CHUNK;
    }
    fflush(pFile);
    fclose(pFile);
    int64_t i64WriteUs = esp_timer_get_time() - i64Start;

    /* Read phase: verify byte by byte to report the exact offset of the first lie */
    pFile = fopen(SDCARD_SUSTAINED_FILE, "rb");
    if (pFile == NULL) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E13) Not able to open file to read back");
        free(pBuffer);
        unlink(SDCARD_SUSTAINED_FILE);
        return -13;
    }

    i64Start = esp_timer_get_time();
    size_t szRead = 0;
    while (szRead < SDCARD_SUSTAINED_SIZE) {
        size_t szChunk = fread(pBuffer, 1, SDCARD_SUSTAINED_CHUNK, pFile);
        if (szChunk == 0) {
            LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E14) Read returned 0 bytes at offset " +
                                std::to_string(szRead) + " - card is unusable for large reads");
            fclose(pFile);
            free(pBuffer);
            unlink(SDCARD_SUSTAINED_FILE);
            return -14;
        }
        for (size_t i = 0; i < szChunk; i++) {
            if (pBuffer[i] != sustainedPatternByte(szRead + i)) {
                LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E15) Data corrupted at offset " +
                                    std::to_string(szRead + i));
                fclose(pFile);
                free(pBuffer);
                unlink(SDCARD_SUSTAINED_FILE);
                return -15;
            }
        }
        szRead += szChunk;
    }
    fclose(pFile);
    int64_t i64ReadUs = esp_timer_get_time() - i64Start;
    free(pBuffer);

    if (unlink(SDCARD_SUSTAINED_FILE) != 0) {
        LogFile.writeToFile(ESP_LOG_ERROR, TAG, "Sustained R/W check: (E16) Unable to delete the file");
        return -16;
    }

    int iWriteKbs = (i64WriteUs > 0) ? (int)((int64_t)(SDCARD_SUSTAINED_SIZE / 1024) * 1000000 / i64WriteUs) : 0;
    int iReadKbs = (i64ReadUs > 0) ? (int)((int64_t)(SDCARD_SUSTAINED_SIZE / 1024) * 1000000 / i64ReadUs) : 0;
    LogFile.writeToFile(ESP_LOG_INFO, TAG, "Sustained R/W check successful (write " +
                        std::to_string(iWriteKbs) + " kB/s, read " + std::to_string(iReadKbs) + " kB/s)");
    return 0;
}
