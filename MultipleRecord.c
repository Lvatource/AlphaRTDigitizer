#include <windows.h>
#include <stdio.h>
#define COR 1

#include "CsPrototypes.h"
#include "CsAppSupport.h"
#include "CsTchar.h"

int64 TransferTimeStamp(CSHANDLE hSystem, uInt32 u32SegmentStart, uInt32 u32SegmentCount, void* pTimeStamp);

int _tmain()
{
	int32						Status = CS_SUCCESS;
	int						    i, j, index, window = 5, z, t;
	double						sum;
	bool						first = 1;
	uInt32						u32ChannelNumber;
	TCHAR						szFileName[MAX_PATH];
	TCHAR						szFormatString[MAX_PATH];
	int64						i64StartOffset = 0;
	void* pBuffer = NULL; //original buffer
	float* pVBuffer = NULL; //buffer with data in volts
	double* pIOBuffer = NULL; //Big buffer that stores all the data, and will be converted into a txt file
	int64* pTimeStamp = NULL;
	double* pDoubleData; //buffer with data in dec/hex
	double* pCorrelationBuffer;
	double* pMathBuffer;
	uInt32						u32Count; //segment count
	int64						i64TickFrequency = 0;
	uInt32						u32Mode;
	CSHANDLE					hSystem = 0;
	IN_PARAMS_TRANSFERDATA		InData = { 0 };
	OUT_PARAMS_TRANSFERDATA		OutData = { 0 };
	CSSYSTEMINFO				CsSysInfo = { 0 };
	CSAPPLICATIONDATA			CsAppData = { 0 };
	FileHeaderStruct			stHeader = { 0 };
	CSACQUISITIONCONFIG			CsAcqCfg = { 0 };
	CSCHANNELCONFIG				CsChanCfg = { 0 };
	LPCTSTR						szIniFile = _T("MultipleRecord.ini");
	uInt32						u32ChannelIndexIncrement;
	int							nMaxSegmentNumber;
	int							nMaxChannelNumber;
	int64						i64Padding = 64; //extra samples to capture to ensure we get what we ask for	
	int64						i64SavedLength;
	int64						i64MaxLength;
	int64						i64MinSA; //Minimum starting address.


	// Initializes the CompuScope boards found in the system. If the
	// system is not found a message with the error code will appear.
	// Otherwise i32Status will contain the number of systems found.
	printf("\nGetting started!\n\n");
	Status = CsInitialize();

	if (CS_FAILED(Status))
	{
		DisplayErrorString(Status);
		return (-1);
	}
	printf("Initiallized the CompuScope boards\n");

	// Get System. This sample program only supports one system. If
	// 2 systems or more are found, the first system that is found
	// will be the system that will be used. hSystem will hold a unique
	// system identifier that is used when referencing the system.
	Status = CsGetSystem(&hSystem, 0, 0, 0, 0);

	if (CS_FAILED(Status))
	{
		DisplayErrorString(Status);
		return (-1);
	}
	printf("Found the system");

	// Get System information. The u32Size field must be filled in
	// prior to calling CsGetSystemInfo
	CsSysInfo.u32Size = sizeof(CSSYSTEMINFO);
	Status = CsGetSystemInfo(hSystem, &CsSysInfo);

	// Display the system name from the driver
	_ftprintf(stdout, _T("\nBoard Identificator: %s"), CsSysInfo.strBoardName);

	// In this example we're only using 1 trigger source
	Status = CsAs_ConfigureSystem(hSystem, (int)CsSysInfo.u32ChannelCount, 1, (LPCTSTR)szIniFile, &u32Mode);

	if (CS_FAILED(Status))
	{
		if (CS_INVALID_FILENAME == Status)
		{
			// Display message but continue on using defaults.
			_ftprintf(stdout, _T("\nCannot find %s - using default parameters."), szIniFile);
		}
		else
		{
			// Otherwise the call failed.  If the call did fail we should free the CompuScope
			// system so it's available for another application
			DisplayErrorString(Status);
			CsFreeSystem(hSystem);
			return(-1);
		}
	}
	printf("\nConfigured the system by the INI file parameters");

	// If the return value is greater than  1, then either the application, 
	// acquisition, some of the Channel and / or some of the Trigger sections
	// were missing from the ini file and the default parameters were used. 
	if (CS_USING_DEFAULT_ACQ_DATA & Status)
		_ftprintf(stdout, _T("\nNo ini entry for acquisition. Using defaults."));

	if (CS_USING_DEFAULT_CHANNEL_DATA & Status)
		_ftprintf(stdout, _T("\nNo ini entry for one or more Channels. Using defaults for missing items."));

	if (CS_USING_DEFAULT_TRIGGER_DATA & Status)
		_ftprintf(stdout, _T("\nNo ini entry for one or more Triggers. Using defaults for missing items."));

	Status = CsAs_LoadConfiguration(hSystem, szIniFile, APPLICATION_DATA, &CsAppData);

	if (CS_FAILED(Status))
	{
		if (CS_INVALID_FILENAME == Status)
			_ftprintf(stdout, _T("\nUsing default application parameters."));
		else
		{
			DisplayErrorString(Status);
			CsFreeSystem(hSystem);
			return -1;
		}
	}
	else if (CS_USING_DEFAULT_APP_DATA & Status)
	{
		// If the return value is CS_USING_DEFAULT_APP_DATA (defined in ConfigSystem.h) 
		// then there was no entry in the ini file for Application and we will use
		// the application default values, which were set in CsAs_LoadConfiguration.
		_ftprintf(stdout, _T("\nNo ini entry for application data. Using defaults"));
	}

	// Commit the values to the driver.  This is where the values get sent to the
	// hardware.  Any invalid parameters will be caught here and an error returned.
	Status = CsDo(hSystem, ACTION_COMMIT);
	if (CS_FAILED(Status))
	{
		DisplayErrorString(Status);
		CsFreeSystem(hSystem);
		return (-1);
	}
	printf("\nThe values have been commited to the driver");

	// Get the current sample size, resolution and offset parameters from the driver
	// by calling CsGet for the ACQUISTIONCONFIG structure. These values are used
	// when saving the file.
	CsAcqCfg.u32Size = sizeof(CsAcqCfg);
	Status = CsGet(hSystem, CS_ACQUISITION, CS_ACQUISITION_CONFIGURATION, &CsAcqCfg);
	if (CS_FAILED(Status))
	{
		DisplayErrorString(Status);
		CsFreeSystem(hSystem);
		return (-1);
	}

	// We need to allocate a buffer for transferring the data
	pBuffer = VirtualAlloc(NULL, (size_t)((CsAppData.i64TransferLength + i64Padding) * CsAcqCfg.u32SampleSize), MEM_COMMIT, PAGE_READWRITE);
	if (NULL == pBuffer)
	{
		_ftprintf(stderr, _T("\nUnable to allocate memory (pBuffer)\n"));
		CsFreeSystem(hSystem);
		return (-1);
	}

	// We also need to allocate a buffer for transferring the timestamp
	pTimeStamp = (int64*)VirtualAlloc(NULL, (size_t)(CsAppData.u32TransferSegmentCount * sizeof(int64)), MEM_COMMIT, PAGE_READWRITE);
	if (NULL == pTimeStamp)
	{
		_ftprintf(stderr, _T("\nUnable to allocate memory (pTimeStamp)\n"));
		if (NULL != pBuffer)
			VirtualFree(pBuffer, 0, MEM_RELEASE);
		return (-1);
	}
	if (TYPE_FLOAT == CsAppData.i32SaveFormat)
	{
		// Allocate another buffer to pass the data that is going to be converted
		// into voltages
		pVBuffer = (float*)VirtualAlloc(NULL, (size_t)(CsAppData.i64TransferLength * sizeof(float)), MEM_COMMIT, PAGE_READWRITE);
		if (NULL == pVBuffer)
		{
			_ftprintf(stderr, _T("\nUnable to allocate memory (pVBuffer)\n"));
			CsFreeSystem(hSystem);
			if (NULL != pBuffer)
				VirtualFree(pBuffer, 0, MEM_RELEASE);
			if (NULL != pTimeStamp)
				VirtualFree(pTimeStamp, 0, MEM_RELEASE);
			return (-1);
		}
	}
	pDoubleData = (double*)VirtualAlloc(NULL, (size_t)(CsAppData.u32TransferSegmentCount * sizeof(double)), MEM_COMMIT, PAGE_READWRITE);

	pIOBuffer = (double*)VirtualAlloc(NULL, (size_t)(CsAppData.i64TransferLength * sizeof(double) * CsAppData.u32TransferSegmentCount), MEM_COMMIT, PAGE_READWRITE);
	if (NULL == pIOBuffer)
	{
		_ftprintf(stderr, _T("\nUnable to allocate memory (pIOBuffer)\n"));
		CsFreeSystem(hSystem);
		return (-1);
	}

	pCorrelationBuffer = (double*)VirtualAlloc(NULL, (size_t)(CsAppData.i64TransferLength * sizeof(double)), MEM_COMMIT, PAGE_READWRITE);
	if (NULL == pIOBuffer)
	{
		_ftprintf(stderr, _T("\nUnable to allocate memory (pCorrelationBuffer)\n"));
		CsFreeSystem(hSystem);
		return (-1);
	}

	pMathBuffer = (double*)VirtualAlloc(NULL, (size_t)(CsAppData.i64TransferLength * sizeof(double)), MEM_COMMIT, PAGE_READWRITE);
	if (NULL == pIOBuffer)
	{
		_ftprintf(stderr, _T("\nUnable to allocate memory (pMathBuffer)\n"));
		CsFreeSystem(hSystem);
		return (-1);
	}

	for (i = 1; i <= 1; i++) {

		// Start the data Acquisition
		Status = CsDo(hSystem, ACTION_START);
		if (CS_FAILED(Status))
		{
			DisplayErrorString(Status);
			CsFreeSystem(hSystem);
			return (-1);
		}

		// DataCaptureComplete queries the system to see when
		// the capture is complete
		if (!DataCaptureComplete(hSystem))
		{
			CsFreeSystem(hSystem);
			return (-1);
		}

		// Acquisition is now complete.
		// Call the function TransferTimeStamp. This function is used to transfer the timestamp
		// data. The i64TickFrequency, which is returned from this function, is the clock rate
		// of the counter used to acquire the timestamp data.
		i64TickFrequency = TransferTimeStamp(hSystem, CsAppData.u32TransferSegmentStart, CsAppData.u32TransferSegmentCount, pTimeStamp);

		// If TransferTimeStamp fails, i32TickFrequency will be negative,
		// which represents an error code. If there is an error we'll set
		// the time stamp info in pDoubleData to 0.
		ZeroMemory(pDoubleData, CsAppData.u32TransferSegmentCount * sizeof(double));
		if (CS_SUCCEEDED(i64TickFrequency))
		{

			// Allocate a buffer of doubles to store the the timestamp data after we have
			// converted it to microseconds.
			for (u32Count = 0; u32Count < CsAppData.u32TransferSegmentCount; u32Count++)
			{
				// The number of ticks that have ocurred / tick count(the number of ticks / second)
				// = the number of seconds elapsed. Multiple by 1000000 to get the number of mircoseconds
				pDoubleData[u32Count] = (double)(*(pTimeStamp + u32Count)) * 1.e6 / (double)(i64TickFrequency);
			}
		}

		// Now transfer the actual acquired data for each desired multiple group.
		// Fill in the InData structure for transferring the data
		InData.u32Mode = TxMODE_DEFAULT;

		// Validate the start address and the length.  This is especially necessary if
		// trigger delay is being used
		i64MinSA = CsAcqCfg.i64TriggerDelay + CsAcqCfg.i64Depth - CsAcqCfg.i64SegmentSize;
		if (CsAppData.i64TransferStartPosition < i64MinSA)
		{
			_ftprintf(stdout, _T("\nInvalid Start Address was changed from %I64i to %I64i\n"), CsAppData.i64TransferStartPosition, i64MinSA);
			CsAppData.i64TransferStartPosition = i64MinSA;
		}
		i64MaxLength = CsAcqCfg.i64TriggerDelay + CsAcqCfg.i64Depth - i64MinSA;
		if (CsAppData.i64TransferLength > i64MaxLength)
		{
			_ftprintf(stdout, _T("\nInvalid Transfer Length was changed from %I64i to %I64i\n"), CsAppData.i64TransferLength, i64MaxLength);
			CsAppData.i64TransferLength = i64MaxLength;
		}

		InData.pDataBuffer = pBuffer;
		u32ChannelIndexIncrement = CsAs_CalculateChannelIndexIncrement(&CsAcqCfg, &CsSysInfo);


		// format a string with the number of segments  and channels so all filenames will have
		// the same number of characters.
		_stprintf(szFormatString, _T("%d"), CsAppData.u32TransferSegmentStart + CsAppData.u32TransferSegmentCount - 1);
		nMaxSegmentNumber = (int)_tcslen(szFormatString);

		_stprintf(szFormatString, _T("%d"), CsSysInfo.u32ChannelCount);
		nMaxChannelNumber = (int)_tcslen(szFormatString);

		_stprintf(szFormatString, _T("%%s_CH%%0%dd-%%0%dd.dat"), nMaxChannelNumber, nMaxSegmentNumber);

		for (u32ChannelNumber = 1; u32ChannelNumber <= CsSysInfo.u32ChannelCount; u32ChannelNumber += u32ChannelIndexIncrement)
		{
			int nMulRecGroup;
			int nTimeStampIndex;

			// Variable that will contain either raw data or data in Volts depending on requested format
			double* pSrcBuffer = NULL;

			ZeroMemory(pBuffer, (size_t)((CsAppData.i64TransferLength + i64Padding) * CsAcqCfg.u32SampleSize));
			InData.u16Channel = (uInt16)u32ChannelNumber;

			// This for loop transfers each multiple record segment to a seperate file. It also
			// writes the time stamp information for the segment to the header of the file. Note
			// that the timestamp array (pDoubleData) starts at index 0, even if the starting transfer
			// segment is not 0. Note that the user is responsible for ensuring that the ini file
			// has valid values and the segments that are being tranferred have been captured.
			//printf("%i", (size_t)(CsAppData.i64TransferLength * sizeof(float)));
			j = 0; /**************************************************************************************************/
			for (nMulRecGroup = CsAppData.u32TransferSegmentStart, nTimeStampIndex = 0; nMulRecGroup < (int)(CsAppData.u32TransferSegmentStart + CsAppData.u32TransferSegmentCount);
				nMulRecGroup++, nTimeStampIndex++) //For each segment:
			{
				// Transfer the captured data
				InData.i64StartAddress = CsAppData.i64TransferStartPosition;
				InData.i64Length = CsAppData.i64TransferLength + i64Padding;
				InData.u32Segment = (uInt32)nMulRecGroup;
				Status = CsTransfer(hSystem, &InData, &OutData);
				if (CS_FAILED(Status))
				{
					DisplayErrorString(Status);
					if (NULL != pBuffer)
						VirtualFree(pBuffer, 0, MEM_RELEASE);
					if (NULL != pVBuffer)
						VirtualFree(pVBuffer, 0, MEM_RELEASE);
					if (NULL != pTimeStamp)
						VirtualFree(pTimeStamp, 0, MEM_RELEASE);
					if (NULL != pDoubleData)
						VirtualFree(pDoubleData, 0, MEM_RELEASE);
					CsFreeSystem(hSystem);
					return (-1);
				}

				// Assign a file name for each channel that we want to save
				_stprintf(szFileName, szFormatString, CsAppData.lpszSaveFileName, u32ChannelNumber, nMulRecGroup);

				// Gather up the information needed for the volt conversion and/or file header
				CsChanCfg.u32Size = sizeof(CSCHANNELCONFIG);
				CsChanCfg.u32ChannelIndex = u32ChannelNumber;
				CsGet(hSystem, CS_CHANNEL, CS_ACQUISITION_CONFIGURATION, &CsChanCfg);
				i64StartOffset = InData.i64StartAddress - OutData.i64ActualStart;
				if (i64StartOffset < 0)
				{
					i64StartOffset = 0;
					InData.i64StartAddress = OutData.i64ActualStart;
				}

				// Save the smaller of the requested transfer length or the actual transferred length
				i64SavedLength = CsAppData.i64TransferLength <= OutData.i64ActualLength ? CsAppData.i64TransferLength : OutData.i64ActualLength;
				i64SavedLength -= i64StartOffset;

				if (TYPE_FLOAT == CsAppData.i32SaveFormat)
				{
					// Call the ConvertToVolts function. This function will convert the raw
					// data to voltages. We pass the buffer plus the actual start, which will be converted
					// from requested start to actual length in the volts buffer.  
					Status = CsAs_ConvertToVolts(i64SavedLength, CsChanCfg.u32InputRange, CsAcqCfg.u32SampleSize,
						CsAcqCfg.i32SampleOffset, CsAcqCfg.i32SampleRes,
						CsChanCfg.i32DcOffset, (void*)((unsigned char*)pBuffer + (i64StartOffset * CsAcqCfg.u32SampleSize)),
						pVBuffer);
					if (CS_FAILED(Status))
					{
						DisplayErrorString(Status);
						continue;
					}
					pSrcBuffer = (void*)(pVBuffer);
				}
				else
				{
					pSrcBuffer = (void*)((unsigned char*)pBuffer + (i64StartOffset * CsAcqCfg.u32SampleSize));
				}
				for (index = 0; index < CsAppData.i64TransferLength / 2; index++) {
					*(pIOBuffer + index + j) = *(pSrcBuffer + index);
					if (first) *(pCorrelationBuffer + index) = *(pSrcBuffer + index);
					double a = *(pCorrelationBuffer + index);
				}
				if (COR) {
					if (first) {
						first = 0;
					}
					else {
						int p, l;
						for (p = 0; p < CsAppData.i64TransferLength / 2; p++) {
							double sum = 0;
							for (l = 0; l < window; l++) {
								if ((p + l) < (CsAppData.i64TransferLength / 2)) {
									double add = (*(pCorrelationBuffer + p + l) * 100) * (*(pSrcBuffer + p + l) * 100);
									sum += add;
								}
							}
							*(pIOBuffer + p + j) = sum;
						}
					}
				}
				j += CsAppData.i64TransferLength / 2;
			}
			// We can put the requested Start Address in the header because any alignment issues have been
			// handled in the buffer.
			stHeader.i64SampleRate = CsAcqCfg.i64SampleRate;
			stHeader.i64Start = InData.i64StartAddress;
			stHeader.i64Length = i64SavedLength;
			stHeader.u32SampleSize = CsAcqCfg.u32SampleSize;
			stHeader.i32SampleRes = CsAcqCfg.i32SampleRes;
			stHeader.u32SampleBits = CsAcqCfg.u32SampleBits;
			stHeader.i32SampleOffset = CsAcqCfg.i32SampleOffset;
			stHeader.u32InputRange = CsChanCfg.u32InputRange;
			stHeader.i32DcOffset = CsChanCfg.i32DcOffset;

			// For sig files we treat each multiple record segment of the capture as a separate file
			//stHeader.u32SegmentCount = (TYPE_SIG == CsAppData.i32SaveFormat) ? 1 : CsAcqCfg.u32SegmentCount;
			//stHeader.u32SegmentNumber = (TYPE_SIG == CsAppData.i32SaveFormat) ? 1 : InData.u32Segment;
			//stHeader.dTimeStamp = pDoubleData[0];

			//pSrcBuffer = CsAppData.u32TransferSegmentStart; //Test: update data.

			Status = (int32)CsAs_SaveFile(szFileName, pIOBuffer, CsAppData.i32SaveFormat, &stHeader, CsAppData.u32TransferSegmentCount);
			if (0 > Status)
			{
				if (CS_MISC_ERROR == Status)
				{
					_ftprintf(stderr, _T("\nFile Error: "));
					_ftprintf(stderr, CsAs_GetLastFileError());
				}
				else
				{
					DisplayErrorString(Status);
					continue;
				}
			}
		}
	}

	// Free any buffers that have been allocated
	if (NULL != pTimeStamp)
	{
		VirtualFree(pTimeStamp, 0, MEM_RELEASE);
		pTimeStamp = NULL;
	}

	if (NULL != pDoubleData)
	{
		VirtualFree(pDoubleData, 0, MEM_RELEASE);
		pDoubleData = NULL;
	}

	if (NULL != pVBuffer)
	{
		VirtualFree(pVBuffer, 0, MEM_RELEASE);
		pVBuffer = NULL;
	}

	if (NULL != pBuffer)
	{
		VirtualFree(pBuffer, 0, MEM_RELEASE);
		pBuffer = NULL;
	}

	// Free the CompuScope system and any resources it's been using
	Status = CsFreeSystem(hSystem);
	if (COR) printf("\nCorrelation- Enabled: window size %d\n", window);
	else printf("\nCorrelation- Disabled\n");

	DisplayFinishString(CsAppData.i32SaveFormat);
	printf("\n\nAll done!");
	printf("\n=============================================================================\n\n");
	return 0;
}

int64 TransferTimeStamp(CSHANDLE hSystem, uInt32 u32SegmentStart, uInt32 u32SegmentCount, void* pTimeStamp)
{
	IN_PARAMS_TRANSFERDATA		InTSData = { 0 };
	OUT_PARAMS_TRANSFERDATA		OutTSData = { 0 };
	int32						i32Status = CS_SUCCESS;
	int64						i64TickFrequency = 0;

	InTSData.u16Channel = 1;
	InTSData.u32Mode = TxMODE_TIMESTAMP;
	InTSData.i64StartAddress = 1;
	InTSData.i64Length = (int64)u32SegmentCount;
	InTSData.u32Segment = u32SegmentStart;

	ZeroMemory(pTimeStamp, (size_t)(u32SegmentCount * sizeof(int64)));
	InTSData.pDataBuffer = pTimeStamp;

	i32Status = CsTransfer(hSystem, &InTSData, &OutTSData);
	if (CS_FAILED(i32Status))
	{
		// if the error is INVALID_TRANSFER_MODE it just means that this systems
		// doesn't support time stamp. We can continue on with the program.
		if (CS_INVALID_TRANSFER_MODE == i32Status)
			_ftprintf(stderr, _T("\nTime stamp is not supported in this system.\n"));
		else
			DisplayErrorString(i32Status);

		VirtualFree(pTimeStamp, 0, MEM_RELEASE);
		pTimeStamp = NULL;
		return (i32Status);
	}

	// The i64TickFrequency, which is returned from this funntion, is the clock rate
	// of the counter used to acquire the timestamp data. Note: this replaces the old
	// way of using the value in OutTSData.i32LowPart as the tick frequency, which is
	// limited by 32 bits.
	i32Status = CsGet(hSystem, CS_PARAMS, CS_TIMESTAMP_TICKFREQUENCY, &i64TickFrequency);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(hSystem);
		return (i32Status);
	}
	return i64TickFrequency;
}