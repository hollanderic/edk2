/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of The Linux Foundation nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <Library/VerifiedBootMenu.h>
#include <Library/DrawUI.h>
#include <Library/ArgList.h>
#include <Protocol/EFIScmModeSwitch.h>
#include <Library/PartitionTableUpdate.h>
#include <Protocol/EFIMdtp.h>

#include "BootMagenta.h"
#include "BootStats.h"
#include "BootImage.h"
#include "UpdateDeviceTree.h"


#define MAGENTA_PARAMETER_MAX_LEN 256
#define MAGENTA_VID_QCOM		0
#define MAGENTA_PID_TRAPPER		0

STATIC CHAR8 MagentaSOCCmd[MAGENTA_PARAMETER_MAX_LEN] = {'\0'};
STATIC CHAR8 MagentaFBCmd[MAGENTA_PARAMETER_MAX_LEN] = {'\0'};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutputProtocol = NULL;


STATIC EFI_STATUS InitGraphicsProto() {
	EFI_HANDLE    ConsoleHandle = (EFI_HANDLE)NULL;

	if (GraphicsOutputProtocol == NULL) {
		ConsoleHandle = gST->ConsoleOutHandle;
		if (ConsoleHandle == NULL) {
			DEBUG((EFI_D_ERROR, "Failed to get the handle for the active console input device.\n"));
			return 0;
		}

		gBS->HandleProtocol (
			ConsoleHandle,
			&gEfiGraphicsOutputProtocolGuid,
			(VOID **) &GraphicsOutputProtocol
		);
		if (GraphicsOutputProtocol == NULL) {
			DEBUG((EFI_D_ERROR, "Failed to get the graphics output protocol.\n"));
			return 0;
		}
	}
	return EFI_SUCCESS;
}


EFI_STATUS GetFBInfo(fb_info_t* fb_info) {

	InitGraphicsProto();
	fb_info->height = GraphicsOutputProtocol->Mode->Info->VerticalResolution;
	fb_info->width  = GraphicsOutputProtocol->Mode->Info->HorizontalResolution;
	fb_info->ptr  	= GraphicsOutputProtocol->Mode->FrameBufferBase;
	fb_info->mode 	= 4;

	return EFI_SUCCESS;
}



EFI_STATUS BootMagenta (VOID *ImageBuffer, UINT32 ImageSize, DeviceInfo *DevInfo, CHAR16 *PartitionName, BOOLEAN Recovery)
{

	EFI_STATUS Status;

	MAGENTA_KERNEL MagentaKernel;
	UINT32 DeviceTreeOffset = 0;
	UINT32 RamdiskOffset = 0;
	UINT32 SecondOffset = 0;
	UINT32 KernelSizeActual = 0;
	UINT32 RamdiskSizeActual = 0;
	UINT32 SecondSizeActual = 0;
	struct kernel64_hdr* Kptr = NULL;

	/*Boot Image header information variables*/
	UINT32 KernelSize = 0;
	UINT64 KernelLoadAddr = 0;
	UINT32 RamdiskSize = 0;
	UINT64 RamdiskLoadAddr = 0;
	UINT64 RamdiskEndAddr = 0;
	UINT32 SecondSize = 0;
	UINT64 DeviceTreeLoadAddr = 0;
	UINT32 PageSize = 0;
	UINT32 DtbOffset = 0;
	CHAR8* FinalCmdLine;

	UINT32 out_len = 0;
	UINT64 out_avai_len = 0;
	CHAR8* CmdLine = NULL;
	UINT64 BaseMemory = 0;
	CHAR8 FfbmStr[FFBM_MODE_BUF_SIZE] = {'\0'};

	if (!StrnCmp(PartitionName, L"boot", StrLen(L"boot")))
	{
		Status = GetFfbmCommand(FfbmStr, FFBM_MODE_BUF_SIZE);
		if (Status != EFI_SUCCESS) {
			DEBUG((EFI_D_INFO, "No Ffbm cookie found, ignore: %r\n", Status));
			FfbmStr[0] = '\0';
		}
	}

	KernelSize = ((boot_img_hdr*)(ImageBuffer))->kernel_size;
	RamdiskSize = ((boot_img_hdr*)(ImageBuffer))->ramdisk_size;
	SecondSize = ((boot_img_hdr*)(ImageBuffer))->second_size;
	PageSize = ((boot_img_hdr*)(ImageBuffer))->page_size;

	CmdLine = (CHAR8*)&(((boot_img_hdr*)(ImageBuffer))->cmdline[0]);

	KernelSizeActual = ROUND_TO_PAGE(KernelSize, PageSize - 1);
	RamdiskSizeActual = ROUND_TO_PAGE(RamdiskSize, PageSize - 1);





	// Retrive Base Memory Address from Ram Partition Table
	Status = BaseMem(&BaseMemory);
	if (Status != EFI_SUCCESS)
	{
		DEBUG((EFI_D_ERROR, "Base memory not found!!! Status:%r\n", Status));
		return Status;
	}



	// These three regions should be reserved in memory map.
	KernelLoadAddr = (EFI_PHYSICAL_ADDRESS)(BaseMemory | PcdGet32(KernelLoadAddress));
	RamdiskLoadAddr = (EFI_PHYSICAL_ADDRESS)(BaseMemory | PcdGet32(RamdiskLoadAddress));
	DeviceTreeLoadAddr = (EFI_PHYSICAL_ADDRESS)(BaseMemory | PcdGet32(TagsAddress));

	DEBUG((EFI_D_INFO, "Base Memory: 0x%lx\n",BaseMemory));
	DEBUG((EFI_D_INFO, "Kernel Load Address: 0x%lx\n",KernelLoadAddr));
	DEBUG((EFI_D_INFO, "RamDisk Load Address: 0x%lx\n",RamdiskLoadAddr));
	DEBUG((EFI_D_INFO, "DeviceTree Load Address: 0x%lx\n",DeviceTreeLoadAddr));



	if (is_gzip_package((ImageBuffer + PageSize), KernelSize))
	{
		// compressed kernel
		out_avai_len = DeviceTreeLoadAddr - KernelLoadAddr;
		if (out_avai_len > MAX_UINT32)
		{
			DEBUG((EFI_D_ERROR, "Integer Oveflow: the length of decompressed data = %u\n", out_avai_len));
			return EFI_BAD_BUFFER_SIZE;
		}

		DEBUG((EFI_D_INFO, "Decompressing kernel image start: %u ms\n", GetTimerCountms()));
		Status = decompress(
				(unsigned char *)(ImageBuffer + PageSize), //Read blob using BlockIo
				KernelSize,                                 //Blob size
				(unsigned char *)KernelLoadAddr,                             //Load address, allocated
				(UINT32)out_avai_len,                               //Allocated Size
				&DtbOffset, &out_len);

		if (Status != EFI_SUCCESS)
		{
			DEBUG((EFI_D_ERROR, "Decompressing kernel image failed!!! Status=%r\n", Status));
			return Status;
		}

		DEBUG((EFI_D_INFO, "Decompressing kernel image done: %u ms\n", GetTimerCountms()));
		Kptr = (struct kernel64_hdr*)KernelLoadAddr;
	} else {
		if (CHECK_ADD64((UINT64)ImageBuffer, PageSize)) {
			DEBUG((EFI_D_ERROR, "Integer Overflow: in Kernel header fields addition\n"));
			return EFI_BAD_BUFFER_SIZE;
		}
		Kptr = ImageBuffer + PageSize;
	}

	CmdLine[BOOT_ARGS_SIZE-1] = '\0';
	DEBUG((EFI_D_ERROR, "Inbound Command Line ->%a\n", CmdLine));



	/*Finds out the location of device tree image and ramdisk image within the boot image
	 *Kernel, Ramdisk and Second sizes all rounded to page
	 *The offset and the LOCAL_ROUND_TO_PAGE function is written in a way that it is done the same in LK*/
	KernelSizeActual = LOCAL_ROUND_TO_PAGE (KernelSize, PageSize);
	RamdiskSizeActual = LOCAL_ROUND_TO_PAGE (RamdiskSize, PageSize);
	SecondSizeActual = LOCAL_ROUND_TO_PAGE (SecondSize, PageSize);

	/*Offsets are the location of the images within the boot image*/
	RamdiskOffset = ADD_OF(PageSize, KernelSizeActual);
	if (!RamdiskOffset)
	{
		DEBUG((EFI_D_ERROR, "Integer Oveflow: PageSize=%u, KernelSizeActual=%u\n",
			PageSize, KernelSizeActual));
		return EFI_BAD_BUFFER_SIZE;
	}

	SecondOffset = ADD_OF(RamdiskOffset, RamdiskSizeActual);
	if (!SecondOffset)
	{
		DEBUG((EFI_D_ERROR, "Integer Oveflow: RamdiskOffset=%u, RamdiskSizeActual=%u\n",
			RamdiskOffset, RamdiskSizeActual));
		return EFI_BAD_BUFFER_SIZE;
	}

	DeviceTreeOffset =  ADD_OF(SecondOffset, SecondSizeActual);
	if (!DeviceTreeOffset)
	{
		DEBUG((EFI_D_ERROR, "Integer Oveflow: SecondOffset=%u, SecondSizeActual=%u\n",
			SecondOffset, SecondSizeActual));
		return EFI_BAD_BUFFER_SIZE;
	}

	DEBUG((EFI_D_VERBOSE, "Kernel Size Actual: 0x%x\n", KernelSizeActual));
	DEBUG((EFI_D_VERBOSE, "Second Size Actual: 0x%x\n", SecondSizeActual));
	DEBUG((EFI_D_VERBOSE, "Ramdisk Size Actual: 0x%x\n", RamdiskSizeActual));
	DEBUG((EFI_D_VERBOSE, "Ramdisk Offset: 0x%x\n", RamdiskOffset));
	DEBUG((EFI_D_VERBOSE, "Device TreeOffset: 0x%x\n", DeviceTreeOffset));

	/* Populate board data required for dtb selection and command line */
	Status = BoardInit();
	if (Status != EFI_SUCCESS)
	{
		DEBUG((EFI_D_ERROR, "Error finding board information: %r\n", Status));
		return Status;
	}


	arg_list_t* magenta_args =NULL;

	/* magenta.soc=vendorid,productid - lets kernel know how to configure root devhost
		for SoC peripherals */
	AsciiSPrint(MagentaSOCCmd, MAGENTA_PARAMETER_MAX_LEN, "magenta.soc=%d,%d", MAGENTA_VID_QCOM, MAGENTA_PID_TRAPPER);

	ArgListNewNode(&magenta_args,MagentaSOCCmd,AsciiStrLen(MagentaSOCCmd));


	/* Add framebuffer details to the command line */
	fb_info_t fb;
	GetFBInfo(&fb);

	AsciiSPrint(MagentaFBCmd, MAGENTA_PARAMETER_MAX_LEN, "magenta.fbuffer=%x,%d,%d,%d",
														fb.ptr, fb.width, fb.height, fb.mode);

	ArgListNewNode(&magenta_args,MagentaFBCmd,AsciiStrLen(MagentaFBCmd));


	/*Append command line parameters passed in from Fastboot*/
	ArgListNewNode(&magenta_args,CmdLine,AsciiStrLen(CmdLine));


	/* Generate the final command line string to pass to the kernel */
	ArgListCat(magenta_args,&FinalCmdLine);

	DEBUG((EFI_D_INFO, "Magenta final command line->%a\n\n\n", FinalCmdLine));

	// appended device tree
	void *dtb;
	dtb = DeviceTreeAppended((void *) (ImageBuffer + PageSize), KernelSize, DtbOffset, (void *)DeviceTreeLoadAddr);
	if (!dtb) {
		DEBUG((EFI_D_ERROR, "Error: Appended Device Tree blob not found\n"));
		return EFI_NOT_FOUND;
	}

	Status = UpdateDeviceTree((VOID*)DeviceTreeLoadAddr , FinalCmdLine, (VOID *)RamdiskLoadAddr, RamdiskSize);
	if (Status != EFI_SUCCESS)
	{
		DEBUG((EFI_D_ERROR, "Device Tree update failed Status:%r\n", Status));
		return Status;
	}

	RamdiskEndAddr = (EFI_PHYSICAL_ADDRESS)(BaseMemory | PcdGet32(RamdiskEndAddress));
	if (RamdiskEndAddr - RamdiskLoadAddr < RamdiskSize){
		DEBUG((EFI_D_ERROR, "Error: Ramdisk size is over the limit\n"));
		return EFI_BAD_BUFFER_SIZE;
	}

	if (CHECK_ADD64((UINT64)ImageBuffer, RamdiskOffset))
	{
		DEBUG((EFI_D_ERROR, "Integer Oveflow: ImageBuffer=%u, RamdiskOffset=%u\n",
			ImageBuffer, RamdiskOffset));
		return EFI_BAD_BUFFER_SIZE;
	}
	CopyMem ((CHAR8*)RamdiskLoadAddr, ImageBuffer + RamdiskOffset, RamdiskSize);



	if (FixedPcdGetBool(EnablePartialGoods))
	{
		Status = UpdatePartialGoodsNode((VOID*)DeviceTreeLoadAddr);
		if (Status != EFI_SUCCESS)
		{
			DEBUG((EFI_D_ERROR, "Failed to update device tree for partial goods, Status=%r\n", Status));
			return Status;
		}
	}

	/* Free the boot logo blt buffer before starting kernel */
	FreeBootLogoBltBuffer();

	DEBUG((EFI_D_INFO, "\nShutting Down UEFI Boot Services: %u ms\n", GetTimerCountms()));
	/*Shut down UEFI boot services*/
	Status = ShutdownUefiBootServices ();
	if(EFI_ERROR(Status)) {
		DEBUG((EFI_D_ERROR,"ERROR: Can not shutdown UEFI boot services. Status=0x%X\n", Status));
		goto Exit;
	}

	Status = PreparePlatformHardware ();
	if (Status != EFI_SUCCESS)
	{
		DEBUG((EFI_D_ERROR,"ERROR: Prepare Hardware Failed. Status:%r\n", Status));
		goto Exit;
	}

	BootStatsSetTimeStamp(BS_KERNEL_ENTRY);
	//
	// Start the Linux Kernel
	//


	MagentaKernel = (MAGENTA_KERNEL)(UINT64)KernelLoadAddr;
	MagentaKernel ((UINT64)DeviceTreeLoadAddr, 0, 0, 0);

	// Kernel should never exit
	// After Life services are not provided

Exit:
	// Only be here if we fail to start Linux
	return EFI_NOT_STARTED;
}
